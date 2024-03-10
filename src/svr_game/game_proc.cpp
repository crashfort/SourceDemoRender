#include "game_proc.h"
#include "game_shared.h"
#include "encoder_shared.h"
#include <d3d11.h>
#include <strsafe.h>
#include <dwrite.h>
#include <d2d1_1.h>
#include <malloc.h>
#include <assert.h>
#include <intrin.h>
#include "svr_prof.h"
#include "game_proc_profile.h"
#include <stb_sprintf.h>
#include "svr_api.h"
#include <Shlwapi.h>
#include <math.h>
#include <float.h>

// Don't use fatal process ending errors in here as this is used both in standalone and in integration.
// It is only standalone SVR that can do fatal processs ending errors.

// -------------------------------------------------

char svr_resource_path[MAX_PATH];

// -------------------------------------------------
// Graphics state.

// High precision texture used for the result of mosample (need to be 32 bits per channel).

ID3D11Texture2D* work_tex;
ID3D11RenderTargetView* work_tex_rtv;
ID3D11ShaderResourceView* work_tex_srv;
ID3D11UnorderedAccessView* work_tex_uav;

// -------------------------------------------------
// Velo state.

ID2D1Factory1* d2d1_factory;
ID2D1Device* d2d1_device;
ID2D1DeviceContext* d2d1_context;
IDWriteFactory* dwrite_factory;
ID2D1SolidColorBrush* d2d1_solid_brush;

IDWriteFontFace* velo_font_face;
ID2D1Bitmap1* velo_rtv_ref; // Not a real texture, but a reference to the used render target.

// Incoming externally.
float player_velo[3];

// -------------------------------------------------
// Mosample state.

__declspec(align(16)) struct MosampleCb
{
    float mosample_weight;
};

ID3D11ComputeShader* mosample_cs;

// Constains the mosample weight.
ID3D11Buffer* mosample_cb;

// To not upload data all the time.
float mosample_weight_cache;
float mosample_remainder;
float mosample_remainder_step;

// -------------------------------------------------
// Movie state.

s32 movie_width;
s32 movie_height;

char movie_path[MAX_PATH];

// -------------------------------------------------
// Time profiling.

SvrProf frame_prof;
SvrProf dl_prof;
SvrProf write_prof;
SvrProf mosample_prof;

// -------------------------------------------------
// Movie profile.

MovieProfile movie_profile;

// -------------------------------------------------
// Encoder state.

HANDLE encoder_proc;
HANDLE encoder_shared_mem_h;
HANDLE game_wake_event_h;
HANDLE encoder_wake_event_h;
EncoderSharedMem* encoder_shared_ptr;
void* encoder_audio_buffer;

// -------------------------------------------------

bool start_encoder_process()
{
    bool ret = false;

    char full_args[1024];
    full_args[0] = 0;

    // Put the handle to the shared memory as a parameter, we can pass the rest in there.
    // All handles are 32-bit, so this is safe for the 64-bit svr_encoder too.
    StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), svr_va("\"%s\\svr_encoder.exe\"", svr_resource_path));
    StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), " ");
    StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), svr_va("%u", (u32)encoder_shared_mem_h));

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION proc_info;

    // Working directory for the encoder process should be in the SVR directory.
    // Need to inherit handles so we can pass the shared memory handle as a parameter.
    if (!CreateProcessA(NULL, full_args, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, svr_resource_path, &start_info, &proc_info))
    {
        svr_log("ERROR: Could not create encoder process (%lu)\n", GetLastError());
        goto rfail;
    }

    // Let the process actually start now.
    // You want to place a breakpoint on this line when debugging svr_encoder!
    // When this breakpoint is hit, attach to the svr_encoder process and then continue this process.
    ResumeThread(proc_info.hThread);

    encoder_proc = proc_info.hProcess;
    CloseHandle(proc_info.hThread);

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool create_encoder_shared_mem()
{
    bool ret = false;

    SECURITY_ATTRIBUTES sa = {};
    sa.bInheritHandle = TRUE; // Allow encoder process to use this handle too.

    s32 mem_size = sizeof(EncoderSharedMem);
    mem_size += sizeof(SvrWaveSample) * ENCODER_MAX_SAMPLES; // Space for audio buffer.

    // Create shared memory handle without a name. The handle will be passed as a parameter to the encoder process
    // and it will open in that way, since we use inherited handles.
    encoder_shared_mem_h = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, mem_size, NULL);

    if (encoder_shared_mem_h == NULL)
    {
        svr_log("ERROR: Could not create encoder shared memory (%lu)\n", GetLastError());
        goto rfail;
    }

    encoder_shared_ptr = (EncoderSharedMem*)MapViewOfFile(encoder_shared_mem_h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

    // This can't fail in this case, but check anyway I guess.
    if (encoder_shared_ptr == NULL)
    {
        svr_log("ERROR: Could not view encoder shared memory (%lu)\n", GetLastError());
        goto rfail;
    }

    // These must be auto reset events so there are no race conditions!
    game_wake_event_h = CreateEventA(&sa, FALSE, FALSE, NULL);
    encoder_wake_event_h = CreateEventA(&sa, FALSE, FALSE, NULL);

    memset(encoder_shared_ptr, 0, mem_size); // Put to known state.

    // Fill some initial data. The encoder process will need these right away.
    // Also build the messy offsets because we are mixing 32-bit and 64-bit.

    encoder_shared_ptr->game_pid = GetCurrentProcessId();
    encoder_shared_ptr->game_wake_event_h = (u32)game_wake_event_h;
    encoder_shared_ptr->encoder_wake_event_h = (u32)encoder_wake_event_h;

    s32 offset = 0;

    offset += sizeof(EncoderSharedMem);
    encoder_shared_ptr->audio_buffer_offset = offset;

    encoder_audio_buffer = (u8*)encoder_shared_ptr + encoder_shared_ptr->audio_buffer_offset;

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool init_encoder()
{
    // The shared memory handle must be created before the encoder process.
    if (!create_encoder_shared_mem())
    {
        return false;
    }

    // Start the encoder process early.
    // The process will always be ready and when movie starts we will notify it that we will send data to it.
    if (!start_encoder_process())
    {
        return false;
    }

    game_log("Started encoder process\n");

    return true;
}

void free_static_encoder()
{
    if (encoder_proc)
    {
        CloseHandle(encoder_proc);
        encoder_proc = NULL;
    }

    if (encoder_shared_mem_h)
    {
        CloseHandle(encoder_shared_mem_h);
        encoder_shared_mem_h = NULL;
        encoder_audio_buffer = NULL;
    }

    if (encoder_shared_ptr)
    {
        UnmapViewOfFile(encoder_shared_ptr);
        encoder_shared_ptr = NULL;
    }

    if (game_wake_event_h)
    {
        CloseHandle(game_wake_event_h);
        game_wake_event_h = NULL;
    }

    if (encoder_wake_event_h)
    {
        CloseHandle(encoder_wake_event_h);
        encoder_wake_event_h = NULL;
    }
}

void free_dynamic_encoder()
{
}

// Call this to resume svr_encoder from a known state.
// You want to call this after you have changed something in the shared memory.
// The variable event_type will be read by svr_encoder once it resumes.
//
// Check the enum for what events can fail. If an event can fail you need to handle it properly by
// checking the return value of this function.
bool send_event_to_encoder(EncoderSharedEvent event)
{
    encoder_shared_ptr->event_type = event;

    SetEvent(encoder_wake_event_h); // Let svr_encoder wake up and handle the event.

    // Block the calling thread until the event has been processed by svr_encoder.
    // We need to do this to ensure the audio and video data access doesn't suffer from any race condition.
    // All the event handling is short and fast so this is a very short wait.
    // When this returns, svr_encoder will be paused and in a known state waiting to be woken up again.
    // This call also makes synchronization easier in svr_game.

    HANDLE handles[] = {
        encoder_proc,
        game_wake_event_h,
    };

    DWORD waited = WaitForMultipleObjects(SVR_ARRAY_SIZE(handles), handles, FALSE, INFINITE);
    HANDLE waited_h = handles[waited - WAIT_OBJECT_0];

    // Encoder exited or crashed or something.
    if (waited_h == encoder_proc)
    {
        game_log("Encoder exited or crashed\n");
        return false;
    }

    if (waited_h == game_wake_event_h)
    {
        if (encoder_shared_ptr->error)
        {
            // Any error in svr_encoder is written to its log.
            // We also want to log the error in the console and in our log.
            game_log(encoder_shared_ptr->error_message);
            return false;
        }
    }

    return true;
}

bool start_encoder(HANDLE game_content_tex_h)
{
    bool ret = false;

    // Set movie parameters to svr_encoder.
    // Audio parameters are fixed in Source so they cannot be changed, just better to have those constants in here.

    EncoderSharedMovieParams* params = &encoder_shared_ptr->movie_params;

    params->video_fps = movie_profile.movie_fps;
    params->video_width = movie_width;
    params->video_height = movie_height;
    params->audio_channels = 2;
    params->audio_hz = 44100;
    params->audio_bits = 16;
    params->x264_crf = movie_profile.sw_x264_crf;
    params->x264_intra = movie_profile.sw_x264_intra;
    params->use_audio = movie_profile.audio_enabled;

    svr_copy_string(movie_path, params->dest_file, SVR_ARRAY_SIZE(params->dest_file));
    svr_copy_string(movie_profile.sw_encoder, params->video_encoder, SVR_ARRAY_SIZE(params->video_encoder));
    svr_copy_string(movie_profile.sw_x264_preset, params->x264_preset, SVR_ARRAY_SIZE(params->x264_preset));
    svr_copy_string(movie_profile.sw_dnxhr_profile, params->dnxhr_profile, SVR_ARRAY_SIZE(params->dnxhr_profile));
    svr_copy_string("aac", params->audio_encoder, SVR_ARRAY_SIZE(params->audio_encoder));

    encoder_shared_ptr->waiting_audio_samples = 0;
    encoder_shared_ptr->game_texture_h = (u32)game_content_tex_h;

    encoder_shared_ptr->error = 0;
    encoder_shared_ptr->error_message[0] = 0;

    // Now wake svr_encoder up and let it wait for new data.
    if (!send_event_to_encoder(ENCODER_EVENT_START))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void end_encoder()
{
    send_event_to_encoder(ENCODER_EVENT_STOP);
}

bool send_frame_to_encoder(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* srv)
{
    bool ret = false;

    // Flush must unfortunately be called when working across processes in order to update
    // the texture in the other process.
    d3d11_context->Flush();

    if (!send_event_to_encoder(ENCODER_EVENT_NEW_VIDEO))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool send_audio_to_encoder(SvrWaveSample* samples, s32 num_samples)
{
    bool ret = false;

    // Don't think this can happen, but let's handle this case anyway because we don't actually know
    // how many samples we can get here at most. Typically though the number of samples will be low (512 or 1024).
    if (num_samples > ENCODER_MAX_SAMPLES)
    {
        // Fragment the writes in case we get too many.
        while (num_samples > 0)
        {
            s32 samples_to_write = svr_min(num_samples, ENCODER_MAX_SAMPLES);

            memcpy(encoder_audio_buffer, samples, sizeof(SvrWaveSample) * samples_to_write);
            encoder_shared_ptr->waiting_audio_samples = samples_to_write;

            if (!send_event_to_encoder(ENCODER_EVENT_NEW_AUDIO))
            {
                goto rfail;
            }

            num_samples -= samples_to_write;
        }
    }

    // Easiest and usual case when everything fits as it shuld.
    else
    {
        memcpy(encoder_audio_buffer, samples, sizeof(SvrWaveSample) * num_samples);
        encoder_shared_ptr->waiting_audio_samples = num_samples;

        if (!send_event_to_encoder(ENCODER_EVENT_NEW_AUDIO))
        {
            goto rfail;
        }
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

// -------------------------------------------------

void update_constant_buffer(ID3D11DeviceContext* d3d11_context, ID3D11Buffer* buffer, void* data, UINT size)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = d3d11_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(hr));

    memcpy(mapped.pData, data, size);

    d3d11_context->Unmap(buffer, 0);
}

// Calculates the thread group count to use for specific dimensions.
s32 calc_cs_thread_groups(s32 input)
{
    // Thread group divisor constant must match the thread count in the compute shaders!

    // This number is arbitrary, and may end up with better or worse performance depending on the hardware.
    return ((float)input / 8.0f) + 0.5f;
}

// -------------------------------------------------

bool load_one_shader(const char* name, void* buf, s32 buf_size, DWORD* shader_size)
{
    char full_shader_path[MAX_PATH];
    full_shader_path[0] = 0;
    StringCchCatA(full_shader_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_shader_path, MAX_PATH, "\\data\\shaders\\");
    StringCchCatA(full_shader_path, MAX_PATH, name);

    HANDLE h = CreateFileA(full_shader_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        svr_log("Could not load shader %s (%lu)\n", name, GetLastError());
        return false;
    }

    ReadFile(h, buf, buf_size, shader_size, NULL);

    CloseHandle(h);

    return true;
}

bool create_a_cs_shader(const char* name, void* file_mem, s32 file_mem_size, ID3D11Device* d3d11_device, ID3D11ComputeShader** cs)
{
    bool ret = false;
    DWORD shader_size;
    HRESULT hr;

    if (!load_one_shader(name, file_mem, file_mem_size, &shader_size))
    {
        goto rfail;
    }

    hr = d3d11_device->CreateComputeShader(file_mem, shader_size, NULL, cs);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create shader %s (%#x)", name, hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void free_all_static_proc_stuff()
{
    svr_maybe_release(&mosample_cs);
    svr_maybe_release(&mosample_cb);

    svr_maybe_release(&d2d1_factory);
    svr_maybe_release(&d2d1_device);
    svr_maybe_release(&d2d1_context);
    svr_maybe_release(&dwrite_factory);
    svr_maybe_release(&d2d1_solid_brush);

    free_static_encoder();
}

void free_all_dynamic_proc_stuff()
{
    svr_maybe_release(&work_tex);
    svr_maybe_release(&work_tex_rtv);
    svr_maybe_release(&work_tex_srv);
    svr_maybe_release(&work_tex_uav);

    svr_maybe_release(&velo_rtv_ref);
    svr_maybe_release(&velo_font_face);

    free_dynamic_encoder();
}

bool proc_init(const char* svr_path, ID3D11Device* d3d11_device)
{
    bool ret = false;
    HRESULT hr;
    D3D11_BUFFER_DESC mosample_cb_desc = {};

    StringCchCopyA(svr_resource_path, MAX_PATH, svr_path);

    IDXGIDevice* dxgi_device;
    d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    // We use D2D1 only for rasterizing vector fonts.

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d1_factory));

    if (FAILED(hr))
    {
        svr_log("ERROR: D2D1CreateFactory returned %#x\n", hr);
        goto rfail;
    }

    hr = d2d1_factory->CreateDevice(dxgi_device, &d2d1_device);

    if (FAILED(hr))
    {
        svr_log("ERROR: ID2D1Factory::CreateDevice returned %#x\n", hr);
        goto rfail;
    }

    hr = d2d1_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d1_context);

    if (FAILED(hr))
    {
        svr_log("ERROR: ID2D1Device::CreateDeviceContext returned %#x\n", hr);
        goto rfail;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create dwrite factory (#x)\n", hr);
        goto rfail;
    }

    d2d1_context->CreateSolidColorBrush({}, &d2d1_solid_brush);

    // Useful for debugging issues with velo rasterization.
    // d2d1_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    mosample_cb_desc.ByteWidth = sizeof(MosampleCb);
    mosample_cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    mosample_cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mosample_cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mosample_cb_desc.MiscFlags = 0;

    hr = d3d11_device->CreateBuffer(&mosample_cb_desc, NULL, &mosample_cb);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create mosample constant buffer (%#x)\n", hr);
        goto rfail;
    }

    if (!init_encoder())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    free_all_static_proc_stuff();

rexit:
    dxgi_device->Release();

    return ret;
}

s32 to_utf16(const char* value, s32 value_length, wchar* buf, s32 buf_chars)
{
    s32 length = MultiByteToWideChar(CP_UTF8, 0, value, value_length, buf, buf_chars);

    if (length < buf_chars)
    {
        buf[length] = 0;
    }

    return length;
}

// Try to find the font in the system.
bool create_velo_font_face(MovieProfile* p, IDWriteFontFace** font_face)
{
    bool ret = false;

    wchar stupid_buf[128];
    to_utf16(p->veloc_font, strlen(p->veloc_font), stupid_buf, 128);

    HRESULT hr;
    IDWriteFontCollection* coll = NULL;
    IDWriteFontFamily* font_fam = NULL;
    IDWriteFont* font = NULL;

    dwrite_factory->GetSystemFontCollection(&coll, FALSE);

    UINT font_index;
    BOOL font_exists;
    coll->FindFamilyName(stupid_buf, &font_index, &font_exists);

    if (!font_exists)
    {
        game_log("The specified velo font %s is not installed in the system\n", p->veloc_font);
        goto rfail;
    }

    coll->GetFontFamily(font_index, &font_fam);

    hr = font_fam->GetFirstMatchingFont(p->veloc_font_weight, DWRITE_FONT_STRETCH_NORMAL, p->veloc_font_style, &font);

    if (FAILED(hr))
    {
        game_log("Could not find the combination of font parameters (weight, stretch, style) in the font %s\n", p->veloc_font);
        goto rfail;
    }

    font->CreateFontFace(font_face);

    ret = true;
    goto rexit;

rfail:
rexit:
    svr_maybe_release(&coll);
    svr_maybe_release(&font_fam);
    svr_maybe_release(&font);

    return ret;
}

bool create_velo(ID3D11Device* d3d11_device, ID3D11DeviceContext* d3d11_context, ID3D11RenderTargetView* used_rtv)
{
    bool ret = false;
    HRESULT hr;

    if (!create_velo_font_face(&movie_profile, &velo_font_face))
    {
        goto rfail;
    }

    ID3D11Resource* content_tex_res;
    used_rtv->GetResource(&content_tex_res);

    ID3D11Texture2D* content_tex;
    content_tex_res->QueryInterface(IID_PPV_ARGS(&content_tex));

    IDXGISurface* content_surface;
    content_tex->QueryInterface(IID_PPV_ARGS(&content_surface));

    // Create passthrough reference to the used render target. This is not a real texture.
    hr = d2d1_context->CreateBitmapFromDxgiSurface(content_surface, NULL, &velo_rtv_ref);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create SRV passthrough (%#x)\n", hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
rexit:
    content_tex_res->Release();
    content_tex->Release();
    content_surface->Release();

    return ret;
}

D2D1_COLOR_F fill_d2d1_color(s32* color)
{
    D2D1_COLOR_F ret;
    ret.r = color[0] / 255.0f;
    ret.g = color[1] / 255.0f;
    ret.b = color[2] / 255.0f;
    ret.a = color[3] / 255.0f;
    return ret;
}

D2D1_POINT_2F fill_d2d1_pt(SvrVec2I p)
{
    return D2D1::Point2F(p.x, p.y);
}

void draw_velo(const wchar* text, SvrVec2I pos)
{
    s32 text_length = wcslen(text);

    d2d1_context->BeginDraw();
    d2d1_context->SetTarget(velo_rtv_ref);

    UINT32* cps = SVR_ALLOCA_NUM(UINT32, text_length);
    UINT16* idxs = SVR_ALLOCA_NUM(UINT16, text_length);
    FLOAT* advances = SVR_ALLOCA_NUM(FLOAT, text_length);
    DWRITE_GLYPH_METRICS* glyph_metrix = SVR_ALLOCA_NUM(DWRITE_GLYPH_METRICS, text_length);

    for (s32 i = 0; i < text_length; i++)
    {
        cps[i] = text[i];
    }

    velo_font_face->GetGlyphIndicesW(cps, text_length, idxs);
    velo_font_face->GetDesignGlyphMetrics(idxs, text_length, glyph_metrix, FALSE);

    DWRITE_FONT_METRICS font_metrix;
    velo_font_face->GetMetrics(&font_metrix);

    float scale = (float)movie_profile.veloc_font_size / (float)font_metrix.designUnitsPerEm;

    // Emulation of tabular font feature where every number is monospaced.
    // For the full feature, the font itself also changes its shaping for the glyphs to be wider, but that is not important.
    // This also prevents the text from jittering when it changes during the centering logic. Typically caused by the 1 character sometimes being thinner than other characters.
    UINT32 tab_cp = L'0';

    UINT16 tab_idx;
    DWRITE_GLYPH_METRICS tab_metrix;

    velo_font_face->GetGlyphIndicesW(&tab_cp, 1, &tab_idx);
    velo_font_face->GetDesignGlyphMetrics(&tab_idx, 1, &tab_metrix, FALSE);

    for (s32 i = 0; i < text_length; i++)
    {
        float aw = tab_metrix.advanceWidth * scale;
        advances[i] = aw;
    }

    // Base horizontal positioning from the center.
    // Vertical positioning is done from the baseline.

    float w = 0.0f;

    for (s32 i = 0; i < text_length; i++)
    {
        w += advances[i];
    }

    s32 real_w = (s32)ceilf(w);

    s32 shift_x = real_w / 2;
    pos.x -= shift_x;

    DWRITE_GLYPH_RUN run = {};
    run.fontFace = velo_font_face;
    run.fontEmSize = movie_profile.veloc_font_size;
    run.glyphCount = text_length;
    run.glyphIndices = idxs;
    run.glyphAdvances = advances;

    ID2D1PathGeometry* geom;
    d2d1_factory->CreatePathGeometry(&geom);

    ID2D1GeometrySink* sink;
    geom->Open(&sink);

    velo_font_face->GetGlyphRunOutline(movie_profile.veloc_font_size, idxs, advances, NULL, text_length, FALSE, FALSE, sink);

    sink->Close();

    d2d1_context->SetTransform(D2D1::Matrix3x2F::Translation(pos.x, pos.y));

    d2d1_solid_brush->SetColor(fill_d2d1_color(movie_profile.veloc_font_color));
    d2d1_context->FillGeometry(geom, d2d1_solid_brush);

    if (movie_profile.veloc_font_border_size > 0)
    {
        d2d1_solid_brush->SetColor(fill_d2d1_color(movie_profile.veloc_font_border_color));
        d2d1_context->DrawGeometry(geom, d2d1_solid_brush, movie_profile.veloc_font_border_size);
    }

    geom->Release();
    sink->Release();

    d2d1_context->EndDraw();
    d2d1_context->SetTarget(NULL);
}

bool proc_start(ID3D11Device* d3d11_device, ID3D11DeviceContext* d3d11_context, const char* dest, const char* profile, ID3D11RenderTargetView* game_content_rtv, void* game_content_tex_h)
{
    bool ret = false;
    HRESULT hr;

    ID3D11RenderTargetView* used_rtv = game_content_rtv;

    if (*profile == 0)
    {
        profile = "default";
    }

    char full_profile_path[MAX_PATH];
    full_profile_path[0] = 0;
    StringCchCatA(full_profile_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_profile_path, MAX_PATH, "\\data\\profiles\\");
    StringCchCatA(full_profile_path, MAX_PATH, profile);
    StringCchCatA(full_profile_path, MAX_PATH, ".ini");

    if (!read_profile(full_profile_path, &movie_profile))
    {
        goto rfail;
    }

    ID3D11Resource* content_tex_res;
    game_content_rtv->GetResource(&content_tex_res);

    ID3D11Texture2D* content_tex;
    content_tex_res->QueryInterface(IID_PPV_ARGS(&content_tex));

    D3D11_TEXTURE2D_DESC tex_desc;
    content_tex->GetDesc(&tex_desc);

    content_tex_res->Release();
    content_tex->Release();

    movie_width = tex_desc.Width;
    movie_height = tex_desc.Height;

    movie_path[0] = 0;
    StringCchCatA(movie_path, MAX_PATH, svr_resource_path);
    StringCchCatA(movie_path, MAX_PATH, "\\movies\\");
    CreateDirectoryA(movie_path, NULL);
    StringCchCatA(movie_path, MAX_PATH, dest);

    if (movie_profile.mosample_enabled)
    {
        D3D11_TEXTURE2D_DESC work_tex_desc = {};
        work_tex_desc.Width = tex_desc.Width;
        work_tex_desc.Height = tex_desc.Height;
        work_tex_desc.MipLevels = 1;
        work_tex_desc.ArraySize = 1;
        work_tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        work_tex_desc.SampleDesc.Count = 1;
        work_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        work_tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        work_tex_desc.CPUAccessFlags = 0;

        hr = d3d11_device->CreateTexture2D(&work_tex_desc, NULL, &work_tex);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create work texture (%#x)\n", hr);
            goto rfail;
        }

        d3d11_device->CreateShaderResourceView(work_tex, NULL, &work_tex_srv);
        d3d11_device->CreateRenderTargetView(work_tex, NULL, &work_tex_rtv);
        d3d11_device->CreateUnorderedAccessView(work_tex, NULL, &work_tex_uav);

        used_rtv = work_tex_rtv;
    }

    if (movie_profile.veloc_enabled)
    {
        if (!create_velo(d3d11_device, d3d11_context, used_rtv))
        {
            goto rfail;
        }
    }

    if (movie_profile.mosample_enabled)
    {
        mosample_remainder = 0.0f;

        s32 sps = movie_profile.movie_fps * movie_profile.mosample_mult;
        mosample_remainder_step = (1.0f / sps) / (1.0f / movie_profile.movie_fps);
    }

    if (!start_encoder(game_content_tex_h))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    free_all_dynamic_proc_stuff();

rexit:
    return ret;
}

void motion_sample(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv, float weight)
{
    if (weight != mosample_weight_cache)
    {
        MosampleCb cb_data;
        cb_data.mosample_weight = weight;

        mosample_weight_cache = weight;
        update_constant_buffer(d3d11_context, mosample_cb, &cb_data, sizeof(MosampleCb));
    }

    d3d11_context->CSSetShader(mosample_cs, NULL, 0);
    d3d11_context->CSSetShaderResources(0, 1, &game_content_srv);

    d3d11_context->CSSetConstantBuffers(0, 1, &mosample_cb);
    d3d11_context->CSSetUnorderedAccessViews(0, 1, &work_tex_uav, NULL);

    d3d11_context->Dispatch(calc_cs_thread_groups(movie_width), calc_cs_thread_groups(movie_height), 1);

    svr_start_prof(&mosample_prof);
    d3d11_context->Flush();
    svr_end_prof(&mosample_prof);

    ID3D11ShaderResourceView* null_srv = NULL;
    ID3D11UnorderedAccessView* null_uav = NULL;

    d3d11_context->CSSetShaderResources(0, 1, &null_srv);
    d3d11_context->CSSetUnorderedAccessViews(0, 1, &null_uav, NULL);
}

// Percentage alignments based from the center of the screen.
SvrVec2I get_velo_pos()
{
    s32 scr_pos_x = movie_width / 2;
    s32 scr_pos_y = movie_height / 2;

    scr_pos_x += (movie_profile.veloc_align[0] / 200.0f) * movie_width;
    scr_pos_y += (movie_profile.veloc_align[1] / 200.0f) * movie_height;

    return SvrVec2I { scr_pos_x, scr_pos_y };
}

void encode_video_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* rtv)
{
    if (movie_profile.veloc_enabled)
    {
        // We only deal with XY velo.
        s32 vel = (s32)(sqrtf(player_velo[0] * player_velo[0] + player_velo[1] * player_velo[1]) + 0.5f);

        wchar buf[128];
        StringCchPrintfW(buf, 128, L"%d", vel);

        draw_velo(buf, get_velo_pos()); // Will draw to the texture of srv and rtv, but that is not apparent.
    }

    send_frame_to_encoder(d3d11_context, srv);
}

void mosample_game_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv)
{
    float old_rem = mosample_remainder;
    float exposure = movie_profile.mosample_exposure;

    mosample_remainder += mosample_remainder_step;

    if (mosample_remainder <= (1.0f - exposure))
    {

    }

    else if (mosample_remainder < 1.0f)
    {
        float weight = (mosample_remainder - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);
        motion_sample(d3d11_context, game_content_srv, weight);
    }

    else
    {
        float weight = (1.0f - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);
        motion_sample(d3d11_context, game_content_srv, weight);

        encode_video_frame(d3d11_context, work_tex_srv, work_tex_rtv);

        mosample_remainder -= 1.0f;

        s32 additional = mosample_remainder;

        if (additional > 0)
        {
            for (s32 i = 0; i < additional; i++)
            {
                encode_video_frame(d3d11_context, work_tex_srv, work_tex_rtv);
            }

            mosample_remainder -= additional;
        }

        float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        d3d11_context->ClearRenderTargetView(work_tex_rtv, clear_color);

        if (mosample_remainder > FLT_EPSILON && mosample_remainder > (1.0f - exposure))
        {
            weight = ((mosample_remainder - (1.0f - exposure)) * (1.0f / exposure));
            motion_sample(d3d11_context, game_content_srv, weight);
        }
    }
}

void proc_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv, ID3D11RenderTargetView* game_content_rtv)
{
    svr_start_prof(&frame_prof);

    if (movie_profile.mosample_enabled)
    {
        mosample_game_frame(d3d11_context, game_content_srv);
    }

    else
    {
        encode_video_frame(d3d11_context, game_content_srv, game_content_rtv);
    }

    svr_end_prof(&frame_prof);
}

void proc_give_velocity(float* xyz)
{
    memcpy(player_velo, xyz, sizeof(float) * 3);
}

bool proc_is_velo_enabled()
{
    return movie_profile.veloc_enabled;
}

bool proc_is_audio_enabled()
{
    return movie_profile.audio_enabled;
}

void proc_give_audio(SvrWaveSample* samples, s32 num_samples)
{
    send_audio_to_encoder(samples, num_samples);
}

void show_total_prof(const char* name, SvrProf* prof)
{
    game_log("%s: %lld\n", name, prof->total);
}

void show_prof(const char* name, SvrProf* prof)
{
    if (prof->runs > 0)
    {
        game_log("%s: %lld\n", name, prof->total / prof->runs);
    }
}

void proc_end()
{
    end_encoder();

    free_all_dynamic_proc_stuff();

    #if SVR_PROF
    show_total_prof("Total work time", &frame_prof);
    show_prof("Download", &dl_prof);
    show_prof("Write", &write_prof);
    show_prof("Mosample", &mosample_prof);
    #endif

    svr_reset_prof(&frame_prof);
    svr_reset_prof(&dl_prof);
    svr_reset_prof(&write_prof);
    svr_reset_prof(&mosample_prof);
}

s32 proc_get_game_rate()
{
    if (movie_profile.mosample_enabled)
    {
        return movie_profile.movie_fps * movie_profile.mosample_mult;
    }

    return movie_profile.movie_fps;
}
