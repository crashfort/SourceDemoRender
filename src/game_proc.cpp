#include "game_proc.h"
#include "game_shared.h"
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <strsafe.h>
#include <dwrite.h>
#include <d2d1_2.h>
#include <malloc.h>
#include <assert.h>
#include <limits>
#include "svr_prof.h"
#include "svr_stream.h"
#include "svr_sem.h"
#include "game_proc_profile.h"
#include "stb_sprintf.h"

// Don't use fatal process ending errors in here as this is used both in standalone and in integration.
// It is only standalone SVR that can do fatal processs ending errors.

// Must be a power of 2.
const s32 NUM_BUFFERED_DL_TEXS = 2;

// Rotation of CPU textures to download to.
struct CpuTexDl
{
    ID3D11Texture2D* texs[NUM_BUFFERED_DL_TEXS];
    s32 i;

    ID3D11Texture2D* get_current()
    {
        return texs[i];
    }

    void advance()
    {
        i = (i + 1) & (NUM_BUFFERED_DL_TEXS - 1);
    }
};

// -------------------------------------------------

char svr_resource_path[MAX_PATH];

// -------------------------------------------------
// Graphics state.

ID3D11PixelShader* texture_ps;
ID3D11VertexShader* overlay_vs;

// High precision texture used for the result of mosample. Need to have high precision (need to be 32 bits per channel).

ID3D11Texture2D* work_tex;
ID3D11RenderTargetView* work_tex_rtv;
ID3D11ShaderResourceView* work_tex_srv;
ID3D11UnorderedAccessView* work_tex_uav;

// -------------------------------------------------
// Velo state.

// V1 is the fastest but does not have any border.
// V2 is slower than V1 and uses geometry instead of text (has no glyph alignment and supports borders).
// V3 is slower than V1 and uses a custom text renderer (has glyph alignment and supports borders).

#define USE_VELO_V1 0
#define USE_VELO_V2 0
#define USE_VELO_V3 1

ID2D1Factory2* d2d1_factory;
ID2D1Device1* d2d1_device;
ID2D1DeviceContext1* d2d1_context;
IDWriteFactory* dwrite_factory;
ID2D1SolidColorBrush* velo_brush;

#if USE_VELO_V2 || USE_VELO_V3
ID2D1SolidColorBrush* velo_border_brush;
#endif

// This texture is linked to the game content / work tex by some means.
ID2D1Bitmap1* d2d1_tex;

#if USE_VELO_V1 || USE_VELO_V3
IDWriteTextFormat* velo_text_format;
#elif USE_VELO_V2
IDWriteFontFace* velo_font_face;
#endif

#if USE_VELO_V3
const s32 NUM_VELO_CACHES = 20000;
ID2D1Geometry** velo_geom_cache;
IDWriteTextLayout** velo_tl_cache;
#endif

float player_velo[3];

#if USE_VELO_V3
struct VeloTextRenderer : IDWriteTextRenderer
{
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject)
    {
        if (__uuidof(IDWriteTextRenderer) == riid)
        {
            *ppvObject = this;
        }

        else if (__uuidof(IDWritePixelSnapping) == riid)
        {
            *ppvObject = this;
        }

        else if (__uuidof(IUnknown) == riid)
        {
            *ppvObject = this;
        }

        else
        {
            *ppvObject = NULL;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }

    ULONG __stdcall AddRef()
    {
        return 0;
    }

    ULONG __stdcall Release()
    {
        return 0;
    }

    HRESULT __stdcall IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled)
    {
        *isDisabled = FALSE;
        return S_OK;
    }

    HRESULT __stdcall GetCurrentTransform(void* clientDrawingContext, DWRITE_MATRIX* transform)
    {
        d2d1_context->GetTransform((D2D1_MATRIX_3X2_F*)transform);
        return S_OK;
    }

    HRESULT __stdcall GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip)
    {
        *pixelsPerDip = 1.0f;
        return S_OK;
    }

    HRESULT __stdcall DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription, IUnknown* clientDrawingEffect)
    {
        s32 real_vel = *(s32*)clientDrawingContext;

        ID2D1Geometry* geom_cache = velo_geom_cache[real_vel];

        if (geom_cache)
        {
            d2d1_context->DrawGeometry(geom_cache, velo_border_brush, 4.0f);
            d2d1_context->FillGeometry(geom_cache, velo_brush);
            return S_OK;
        }

        DWRITE_GLYPH_RUN const* gr = glyphRun;

        ID2D1PathGeometry* geom;
        d2d1_factory->CreatePathGeometry(&geom);

        ID2D1GeometrySink* sink;
        geom->Open(&sink);

        gr->fontFace->GetGlyphRunOutline(gr->fontEmSize, gr->glyphIndices, gr->glyphAdvances, gr->glyphOffsets, gr->glyphCount, FALSE, FALSE, sink);
        sink->Close();

        D2D1_MATRIX_3X2_F transform;
        transform.m11 = 1.0f;
        transform.m12 = 0.0f;
        transform.m21 = 0.0f;
        transform.m22 = 1.0f;
        transform.dx = baselineOriginX;
        transform.dy = baselineOriginY;

        ID2D1TransformedGeometry* trans_geom;
        d2d1_factory->CreateTransformedGeometry(geom, &transform, &trans_geom);

        sink->Release();
        geom->Release();

        velo_geom_cache[real_vel] = trans_geom;
        geom_cache = velo_geom_cache[real_vel];

        d2d1_context->DrawGeometry(geom_cache, velo_border_brush, 4.0f);
        d2d1_context->FillGeometry(geom_cache, velo_brush);
        return S_OK;
    }

    HRESULT __stdcall DrawUnderline(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect)
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall DrawStrikethrough(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect)
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect)
    {
        return E_NOTIMPL;
    }
};
#endif

#if USE_VELO_V3
VeloTextRenderer velo_renderer;
#endif

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

s32 frame_num;
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
// FFmpeg process communication.

struct ThreadPipeData
{
    u8* ptr;
    s32 size;
};

// We write data to the ffmpeg process through this pipe.
// It is redirected to their stdin.
HANDLE ffmpeg_write_pipe;

HANDLE ffmpeg_proc;

// How many completed buffers we keep in memory waiting to be sent to ffmpeg.
const s32 MAX_BUFFERED_SEND_BUFS = 8;

// The buffers that are sent to the ffmpeg process.
// For SW encoding these buffers are uncompressed frames of equal size.
ThreadPipeData ffmpeg_send_bufs[MAX_BUFFERED_SEND_BUFS];

HANDLE ffmpeg_thread;

// Queues and semaphores for communicating between the threads.

SvrAsyncStream<ThreadPipeData> ffmpeg_write_queue;
SvrAsyncStream<ThreadPipeData> ffmpeg_read_queue;

// Semaphore that is signalled when there are frames to send to ffmpeg (pulls from ffmpeg_write_queue).
// This is incremented by the game thread when it has added a downloaded frame to the write queue.
SvrSemaphore ffmpeg_write_sem;

// Semaphore that is signalled when there are frames available to download into (pulls from ffmpeg_read_queue).
// This is incremented by the ffmpeg thread when it has sent a frame to the ffmpeg process.
SvrSemaphore ffmpeg_read_sem;

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
// SW encoding.

struct PxConvText
{
    const char* format;
    const char* color_space;
};

enum PxConv
{
    PXCONV_YUV420_601 = 0,
    PXCONV_YUV444_601,
    PXCONV_NV12_601,
    PXCONV_NV21_601,

    PXCONV_YUV420_709,
    PXCONV_YUV444_709,
    PXCONV_NV12_709,
    PXCONV_NV21_709,

    PXCONV_BGR0,

    NUM_PXCONVS,
};

// Up to 3 planes used for YUV video.
ID3D11Texture2D* pxconv_texs[3];
ID3D11UnorderedAccessView* pxconv_uavs[3];
ID3D11ComputeShader* pxconv_cs[NUM_PXCONVS];

CpuTexDl pxconv_dls[3];

UINT pxconv_pitches[3];
UINT pxconv_widths[3];
UINT pxconv_heights[3];

s32 used_pxconv_planes;
s32 pxconv_plane_sizes[3];
s32 pxconv_total_plane_sizes;

PxConv movie_pxconv;

// All the pxconv arrays are synchronized!

// Names of compiled shaders.
const char* PXCONV_SHADER_NAMES[] = {
    "80789c65c37c6acbf800e0a12e18e0fd4950065b",
    "cf005a5b5fc239779f3cf1e19cf2dab33e503ffc",
    "2a3229bd1f9d4785c87bc7913995d82dc4e09572",
    "58c00ca9b019bbba2dca15ec4c4c9c494ae7d842",

    "38668b40da377284241635b07e22215c204eb137",
    "659f7e14fec1e7018a590c5a01d8169be881438a",
    "b9978bbddaf801c44f71f8efa9f5b715ada90000",
    "5d5924a1a56d4d450743e939d571d04a82209673",

    "b44000e74095a254ef98a2cdfcbaf015ab6c295e",
};

// Names for ini.
PxConvText PXCONV_INI_TEXT_TABLE[] = {
    PxConvText { "yuv420", "601" },
    PxConvText { "yuv444", "601" },
    PxConvText { "nv12", "601" },
    PxConvText { "nv21", "601" },

    PxConvText { "yuv420", "709" },
    PxConvText { "yuv444", "709" },
    PxConvText { "nv12", "709" },
    PxConvText { "nv21", "709" },

    PxConvText { "bgr0", "rgb" },
};

// Names for ffmpeg.
PxConvText PXCONV_FFMPEG_TEXT_TABLE[] = {
    PxConvText { "yuv420p", "bt470bg" },
    PxConvText { "yuv444p", "bt470bg" },
    PxConvText { "nv12", "bt470bg" },
    PxConvText { "nv21", "bt470bg" },

    PxConvText { "yuv420p", "bt709" },
    PxConvText { "yuv444p", "bt709" },
    PxConvText { "nv12", "bt709" },
    PxConvText { "nv21", "bt709" },

    PxConvText { "bgr0", NULL },
};

// How many planes that are used in a pixel format.
s32 calc_format_planes(PxConv pxconv)
{
    switch (pxconv)
    {
        case PXCONV_YUV420_601:
        case PXCONV_YUV444_601:
        case PXCONV_YUV420_709:
        case PXCONV_YUV444_709:
        {
            return 3;
        }

        case PXCONV_NV12_601:
        case PXCONV_NV21_601:
        case PXCONV_NV12_709:
        case PXCONV_NV21_709:
        {
            return 2;
        }

        case PXCONV_BGR0:
        {
            return 1;
        }
    }

    assert(false);
    return 0;
}

UINT calc_bytes_pitch(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8_UINT: return 1;
        case DXGI_FORMAT_R8G8_UINT: return 2;
        case DXGI_FORMAT_R8G8B8A8_UINT: return 4;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
    }

    assert(false);
    return 0;
}

// Retrieves the size of a plane in a pixel format.
void calc_plane_dims(PxConv pxconv, s32 width, s32 height, s32 plane, s32* out_width, s32* out_height)
{
    // Height and width chroma shifts for each plane dimension (luma is ignored, so set to 0).

    const s32 YUV420_SHIFTS[3] = { 0, 1, 1 };
    const s32 NV_SHIFTS[2] = { 0, 1 };

    switch (pxconv)
    {
        case PXCONV_YUV420_601:
        case PXCONV_YUV420_709:
        {
            assert(plane < 3);

            *out_width = width >> YUV420_SHIFTS[plane];
            *out_height = height >> YUV420_SHIFTS[plane];
            break;
        }

        case PXCONV_NV12_601:
        case PXCONV_NV21_601:
        case PXCONV_NV12_709:
        case PXCONV_NV21_709:
        {
            assert(plane < 2);

            *out_width = width >> NV_SHIFTS[plane];
            *out_height = height >> NV_SHIFTS[plane];
            break;
        }

        case PXCONV_YUV444_601:
        case PXCONV_YUV444_709:
        case PXCONV_BGR0:
        {
            assert(plane < 1);

            *out_width = width;
            *out_height = height;
            break;
        }

        default:
        {
            assert(false);
            break;
        }
    }
}

// Put textures into system memory.
// The system memory destination msut be big enough to hold all textures.
void download_textures(ID3D11DeviceContext* d3d11_context, ID3D11Texture2D** gpu_texes, CpuTexDl* cpu_texes, s32 num_texes, void* dest, s32 size)
{
    // Need to copy the textures into readable memory.

    for (s32 i = 0; i < num_texes; i++)
    {
        d3d11_context->CopyResource(cpu_texes[i].get_current(), gpu_texes[i]);
    }

    D3D11_MAPPED_SUBRESOURCE* maps = (D3D11_MAPPED_SUBRESOURCE*)_alloca(sizeof(D3D11_MAPPED_SUBRESOURCE) * num_texes);
    void** map_datas = (void**)_alloca(sizeof(void*) * num_texes);
    UINT* row_pitches = (UINT*)_alloca(sizeof(UINT) * num_texes);

    // Mapping will take between 400 and 1500 us on average, not much to do about that. Probably has to do with waiting for CopyResource above to finish.
    // We cannot use D3D11_MAP_FLAG_DO_NOT_WAIT here (and advance the cpu texture queue) because of the CopyResource call above which
    // may not have finished yet.

    for (s32 i = 0; i < num_texes; i++)
    {
        d3d11_context->Map(cpu_texes[i].get_current(), 0, D3D11_MAP_READ, 0, &maps[i]);
    }

    for (s32 i = 0; i < num_texes; i++)
    {
        map_datas[i] = maps[i].pData;
        row_pitches[i] = maps[i].RowPitch;
    }

    // From MSDN:
    // The runtime might assign values to RowPitch and DepthPitch that are larger than anticipated
    // because there might be padding between rows and depth.

    // Mapped data will be aligned to 16 bytes.

    // This will take around 300 us for 1920x1080 YUV420.

    s32 offset = 0;

    for (s32 i = 0; i < num_texes; i++)
    {
        u8* source_ptr = (u8*)map_datas[i];
        u8* dest_ptr = (u8*)dest + offset;

        for (UINT j = 0; j < pxconv_heights[i]; j++)
        {
            memcpy(dest_ptr, source_ptr, pxconv_pitches[i]);

            source_ptr += row_pitches[i];
            dest_ptr += pxconv_pitches[i];
        }

        offset += pxconv_pitches[i] * pxconv_heights[i];
    }

    for (s32 i = 0; i < num_texes; i++)
    {
        d3d11_context->Unmap(cpu_texes[i].get_current(), 0);
    }

    for (s32 i = 0; i < num_texes; i++)
    {
        cpu_texes[i].advance();
    }
}

void free_all_static_sw_stuff()
{
    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        svr_maybe_release(&pxconv_cs[i]);
    }
}

void free_all_dynamic_sw_stuff()
{
    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        svr_maybe_release(&pxconv_texs[i]);
        svr_maybe_release(&pxconv_uavs[i]);

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            svr_maybe_release(&pxconv_dls[i].texs[j]);
        }
    }

    for (s32 i = 0; i < MAX_BUFFERED_SEND_BUFS; i++)
    {
        ThreadPipeData& pipe_data =  ffmpeg_send_bufs[i];

        if (pipe_data.ptr)
        {
            free(pipe_data.ptr);
        }

        pipe_data = {};
    }
}

// Put texture into video format.
void convert_pixel_formats(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* source_srv)
{
    d3d11_context->CSSetShader(pxconv_cs[movie_pxconv], NULL, 0);
    d3d11_context->CSSetShaderResources(0, 1, &source_srv);
    d3d11_context->CSSetUnorderedAccessViews(0, used_pxconv_planes, pxconv_uavs, NULL);

    d3d11_context->Dispatch(calc_cs_thread_groups(movie_width), calc_cs_thread_groups(movie_height), 1);

    ID3D11ShaderResourceView* null_srvs[] = { NULL };
    ID3D11UnorderedAccessView* null_uavs[] = { NULL };

    d3d11_context->CSSetShaderResources(0, 1, null_srvs);
    d3d11_context->CSSetUnorderedAccessViews(0, 1, null_uavs, NULL);
}

bool create_pxconv_textures(ID3D11Device* d3d11_device, DXGI_FORMAT* formats)
{
    bool ret = false;
    HRESULT hr;

    PxConvText& text = PXCONV_INI_TEXT_TABLE[movie_pxconv];

    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        s32 dims[2];
        calc_plane_dims(movie_pxconv, movie_width, movie_height, i, &dims[0], &dims[1]);

        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = dims[0];
        tex_desc.Height = dims[1];
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = formats[i];
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        tex_desc.CPUAccessFlags = 0;

        hr = d3d11_device->CreateTexture2D(&tex_desc, NULL, &pxconv_texs[i]);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create SWENC texture %d for PXCONV %s+%s (%#x)\n", i, text.format, text.color_space, hr);
            goto rfail;
        }

        d3d11_device->CreateUnorderedAccessView(pxconv_texs[i], NULL, &pxconv_uavs[i]);

        tex_desc.Usage = D3D11_USAGE_STAGING;
        tex_desc.BindFlags = 0;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            hr = d3d11_device->CreateTexture2D(&tex_desc, NULL, &pxconv_dls[i].texs[j]);

            if (FAILED(hr))
            {
                svr_log("ERROR: Could not create SWENC CPU texture %d (rotation %d) for PXCONV %s+%s (%#x)\n", i, j, text.format, text.color_space, hr);
                goto rfail;
            }
        }
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool start_with_sw(ID3D11Device* d3d11_device)
{
    bool ret = false;

    // We don't allow the pixel format and color space to be selectable anymore, but we will keep the support in for now.

    if (!strcmp(movie_profile.sw_encoder, "libx264"))
    {
        movie_pxconv = PXCONV_NV12_601;
    }

    else if (!strcmp(movie_profile.sw_encoder, "libx264rgb"))
    {
        movie_pxconv = PXCONV_BGR0;
    }

    used_pxconv_planes = calc_format_planes(movie_pxconv);

    DXGI_FORMAT formats[3];

    switch (movie_pxconv)
    {
        case PXCONV_YUV420_601:
        case PXCONV_YUV420_709:
        case PXCONV_YUV444_601:
        case PXCONV_YUV444_709:
        {
            formats[0] = DXGI_FORMAT_R8_UINT;
            formats[1] = DXGI_FORMAT_R8_UINT;
            formats[2] = DXGI_FORMAT_R8_UINT;
            break;
        }

        case PXCONV_NV12_601:
        case PXCONV_NV21_601:
        case PXCONV_NV12_709:
        case PXCONV_NV21_709:
        {
            formats[0] = DXGI_FORMAT_R8_UINT;
            formats[1] = DXGI_FORMAT_R8G8_UINT;
            break;
        }

        case PXCONV_BGR0:
        {
            // RGB only has one plane.
            formats[0] = DXGI_FORMAT_R8G8B8A8_UINT;
            break;
        }
    }

    if (!create_pxconv_textures(d3d11_device, formats))
    {
        goto rfail;
    }

    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        D3D11_TEXTURE2D_DESC tex_desc;
        pxconv_texs[i]->GetDesc(&tex_desc);

        pxconv_plane_sizes[i] = calc_bytes_pitch(tex_desc.Format) * tex_desc.Width * tex_desc.Height;
        pxconv_widths[i] = tex_desc.Width;
        pxconv_heights[i] = tex_desc.Height;
        pxconv_pitches[i] = calc_bytes_pitch(tex_desc.Format) * tex_desc.Width;
    }

    // Combined size of all planes.
    pxconv_total_plane_sizes = 0;

    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        pxconv_total_plane_sizes += pxconv_plane_sizes[i];
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

// -------------------------------------------------

// This thread will write data to the ffmpeg process.
// Writing to the pipe is real slow and we want to buffer up a few to send which it can work on.
DWORD WINAPI ffmpeg_thread_proc(LPVOID lpParameter)
{
    while (true)
    {
        svr_sem_wait(&ffmpeg_write_sem);

        ThreadPipeData pipe_data;
        bool res1 = ffmpeg_write_queue.pull(&pipe_data);
        assert(res1);

        // This will not return until the data has been read by the remote process.
        // Writing with pipes is very inconsistent and can wary with several milliseconds.
        // It has been tested to use overlapped I/O with completion routines but that was also too inconsistent and way too complicated.
        // For SW encoding this will take about 300 - 6000 us (always sending the same size).

        // There is some issue with writing with pipes that if it starts off slower than it should be, then it will forever be slow until the computer restarts.
        // Therefore it is useful to measure this.

        svr_start_prof(&write_prof);
        WriteFile(ffmpeg_write_pipe, pipe_data.ptr, pipe_data.size, NULL, NULL);
        svr_end_prof(&write_prof);

        ffmpeg_read_queue.push(&pipe_data);

        svr_sem_release(&ffmpeg_read_sem);
    }
}

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

bool create_shaders(ID3D11Device* d3d11_device)
{
    const s32 SHADER_MEM_SIZE = 8192;

    bool ret = false;
    DWORD shader_size;
    HRESULT hr;

    void* file_mem = malloc(SHADER_MEM_SIZE);

    // Pixel format conversion shaders are only used in SW encoding.

    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        if (!create_a_cs_shader(PXCONV_SHADER_NAMES[i], file_mem, SHADER_MEM_SIZE, d3d11_device, &pxconv_cs[i])) goto rfail;
    }

    if (!create_a_cs_shader("c52620855f15b2c47b8ca24b890850a90fdc7017", file_mem, SHADER_MEM_SIZE, d3d11_device, &mosample_cs)) goto rfail;

    ret = true;
    goto rexit;

rfail:

rexit:
    free(file_mem);

    return ret;
}

void free_all_static_proc_stuff()
{
    svr_maybe_release(&texture_ps);
    svr_maybe_release(&overlay_vs);

    svr_maybe_release(&mosample_cs);

    svr_maybe_release(&mosample_cb);

    svr_maybe_release(&d2d1_factory);
    svr_maybe_release(&d2d1_device);
    svr_maybe_release(&d2d1_context);
    svr_maybe_release(&dwrite_factory);
    svr_maybe_release(&velo_brush);

    #if USE_VELO_V2 || USE_VELO_V3
    svr_maybe_release(&velo_border_brush);
    #endif
}

void free_all_dynamic_proc_stuff()
{
    svr_maybe_release(&work_tex);
    svr_maybe_release(&work_tex_rtv);
    svr_maybe_release(&work_tex_srv);
    svr_maybe_release(&work_tex_uav);

    svr_maybe_release(&d2d1_tex);

    #if USE_VELO_V1 || USE_VELO_V3
    svr_maybe_release(&velo_text_format);
    #elif USE_VELO_V2
    svr_maybe_release(&velo_font_face);
    #endif
}

bool proc_init(const char* svr_path, ID3D11Device* d3d11_device)
{
    bool ret = false;
    HRESULT hr;
    D3D11_BUFFER_DESC mosample_cb_desc = {};

    StringCchCopyA(svr_resource_path, MAX_PATH, svr_path);

    IDXGIDevice* dxgi_device;
    d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d1_factory));

        if (FAILED(hr))
        {
            svr_log("ERROR: D2D1CreateFactory returned %#x\n", hr);
            goto rfail;
        }

        hr = d2d1_factory->CreateDevice(dxgi_device, &d2d1_device);

        if (FAILED(hr))
        {
            svr_log("ERROR: ID2D1Factory7::CreateDevice returned %#x\n", hr);
            goto rfail;
        }

        hr = d2d1_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &d2d1_context);

        if (FAILED(hr))
        {
            svr_log("ERROR: ID2D1Device6::CreateDeviceContext returned %#x\n", hr);
            goto rfail;
        }

        #if USE_VELO_V1 || USE_VELO_V3
        // We want grayscale AA for velo because of the moving background.
        d2d1_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        #endif

        #if USE_VELO_V3
        d2d1_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        #endif

        // We are not a desktop application and users will be expecting pixel offsets for velo text.
        d2d1_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create dwrite factory (#x)\n", hr);
            goto rfail;
        }

        d2d1_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &velo_brush);

        #if USE_VELO_V2 || USE_VELO_V3
        d2d1_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &velo_border_brush);
        #endif
    }

    IDXGIAdapter* dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);

    DXGI_ADAPTER_DESC dxgi_adapter_desc;
    dxgi_adapter->GetDesc(&dxgi_adapter_desc);

    dxgi_adapter->Release();

    // Useful for future troubleshooting.
    // Use https://www.pcilookup.com/ to see more information about device and vendor ids.
    svr_log("Using graphics device %x by vendor %x\n", dxgi_adapter_desc.DeviceId, dxgi_adapter_desc.VendorId);

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

    if (!create_shaders(d3d11_device))
    {
        goto rfail;
    }

    svr_sem_init(&ffmpeg_write_sem, 0, MAX_BUFFERED_SEND_BUFS);
    svr_sem_init(&ffmpeg_read_sem, MAX_BUFFERED_SEND_BUFS, MAX_BUFFERED_SEND_BUFS);

    ffmpeg_write_queue.init(MAX_BUFFERED_SEND_BUFS);
    ffmpeg_read_queue.init(MAX_BUFFERED_SEND_BUFS);

    // This thread never exits.
    ffmpeg_thread = CreateThread(NULL, 0, ffmpeg_thread_proc, NULL, 0, NULL);

    ret = true;
    goto rexit;

rfail:
    free_all_static_proc_stuff();

rexit:
    dxgi_device->Release();

    return ret;
}

void build_ffmpeg_process_args(char* full_args, s32 full_args_size)
{
    const s32 ARGS_BUF_SIZE = 128;

    char buf[ARGS_BUF_SIZE];

    StringCchCatA(full_args, full_args_size, "-hide_banner");

    #if SVR_RELEASE
    StringCchCatA(full_args, full_args_size, " -loglevel quiet");
    #else
    StringCchCatA(full_args, full_args_size, " -loglevel debug");
    #endif

    // Parameters below here is regarding the input (the stuff we are sending).

    PxConvText& pxconv_text = PXCONV_FFMPEG_TEXT_TABLE[movie_pxconv];

    // We are sending uncompressed frames.
    StringCchCatA(full_args, full_args_size, " -f rawvideo -vcodec rawvideo");

    // Pixel format that goes through the pipe.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -pix_fmt %s", pxconv_text.format);
    StringCchCatA(full_args, full_args_size, buf);

    // Video size.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -s %dx%d", movie_width, movie_height);
    StringCchCatA(full_args, full_args_size, buf);

    // Input frame rate.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -r %d", movie_profile.movie_fps);
    StringCchCatA(full_args, full_args_size, buf);

    // Overwrite existing, and read from stdin.
    StringCchCatA(full_args, full_args_size, " -y -i -");

    // Parameters below here is regarding the output (the stuff that will be written to the file).

    // Number of encoding threads, or 0 for auto.
    // We used to allow this to be configured, its intended purpose was for game multiprocessing (opening multiple games) but
    // there are too many problems in the Source engine that we cannot control. It leads to many buggy and weird scenarios (like animations not playing or demos jumping).
    StringCchCatA(full_args, full_args_size, " -threads 0");

    // Output video codec.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -vcodec %s", movie_profile.sw_encoder);
    StringCchCatA(full_args, full_args_size, buf);

    if (pxconv_text.color_space)
    {
        // Output video color space (only for YUV).
        StringCchPrintfA(buf, ARGS_BUF_SIZE, " -colorspace %s", pxconv_text.color_space);
        StringCchCatA(full_args, full_args_size, buf);
    }

    // Output video framerate.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -framerate %d", movie_profile.movie_fps);
    StringCchCatA(full_args, full_args_size, buf);

    // Output quality factor.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -crf %d", movie_profile.sw_crf);
    StringCchCatA(full_args, full_args_size, buf);

    // Output x264 preset.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -preset %s", movie_profile.sw_x264_preset);
    StringCchCatA(full_args, full_args_size, buf);

    if (movie_profile.sw_x264_intra)
    {
        StringCchCatA(full_args, full_args_size, " -x264-params keyint=1");
    }

    // The path can be specified as relative here because we set the working directory of the ffmpeg process
    // to the SVR directory.

    StringCchCatA(full_args, full_args_size, " \"");
    StringCchCatA(full_args, full_args_size, movie_path);
    StringCchCatA(full_args, full_args_size, "\"");
}

// We start a separate process for two reasons:
// 1) Source is a 32-bit engine, and it was common to run out of memory in games such as CSGO that uses a lot of memory.
// 2) The ffmpeg API is horrible to work with with an incredible amount of pitfalls that will grant you a media that is slighly incorrect
//    and there is no reliable documentation.
// Data is sent to this process through a pipe that we create.
// For SW encoding we send uncompressed frames which are then encoded and muxed in the ffmpeg process.
bool start_ffmpeg_proc()
{
    const s32 FULL_ARGS_SIZE = 1024;

    bool ret = false;

    STARTUPINFOA start_info = {};
    DWORD create_flags = 0;

    char full_args[FULL_ARGS_SIZE];
    full_args[0] = 0;

    HANDLE read_h = NULL;
    HANDLE write_h = NULL;

    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION proc_info;

    char full_ffmpeg_path[MAX_PATH];

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&read_h, &write_h, &sa, 4 * 1024))
    {
        svr_log("ERROR: Could not create ffmpeg process pipes (%lu)\n", GetLastError());
        goto rfail;
    }

    // Since we start the ffmpeg process with inherited handles, it would try to inherit the writing endpoint of this pipe too.
    // We have to remove the inheritance of this pipe. Otherwise it's never able to exit.
    SetHandleInformation(write_h, HANDLE_FLAG_INHERIT, 0);

    #if SVR_RELEASE
    create_flags |= CREATE_NO_WINDOW;
    #endif

    // Working directory for the FFmpeg process should be in the SVR directory.

    full_ffmpeg_path[0] = 0;
    StringCchCatA(full_ffmpeg_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_ffmpeg_path, MAX_PATH, "\\ffmpeg.exe");

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = read_h;
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    build_ffmpeg_process_args(full_args, FULL_ARGS_SIZE);

    svr_log("Starting ffmpeg with args %s\n", full_args);

    if (!CreateProcessA(full_ffmpeg_path, full_args, NULL, NULL, TRUE, create_flags, NULL, svr_resource_path, &start_info, &proc_info))
    {
        svr_log("ERROR: Could not create ffmpeg process (%lu)\n", GetLastError());
        goto rfail;
    }

    ffmpeg_proc = proc_info.hProcess;
    CloseHandle(proc_info.hThread);

    ffmpeg_write_pipe = write_h;

    ret = true;
    goto rexit;

rfail:
    if (write_h) CloseHandle(write_h);

rexit:
    if (read_h) CloseHandle(read_h);

    return ret;
}

void end_ffmpeg_proc()
{
    while (ffmpeg_write_queue.read_buffer_health() > 0)
    {
        // Stupid but we can't use any other mechanism with the ffmpeg thread loop.
        Sleep(100);
    }

    // Both queues should not be updated anymore at this point so they should be the same when observed.
    assert(ffmpeg_write_queue.read_buffer_health() == 0);
    assert(ffmpeg_read_queue.read_buffer_health() == MAX_BUFFERED_SEND_BUFS);

    // Close our end of the pipe.
    // This will mark the completion of the stream, and the process will finish its work.
    CloseHandle(ffmpeg_write_pipe);
    ffmpeg_write_pipe = NULL;

    WaitForSingleObject(ffmpeg_proc, INFINITE);

    CloseHandle(ffmpeg_proc);
    ffmpeg_proc = NULL;
}

s32 to_utf16(const char* value, s32 value_length, wchar* buf, s32 buf_chars)
{
    auto length = MultiByteToWideChar(CP_UTF8, 0, value, value_length, buf, buf_chars);

    if (length < buf_chars)
    {
        buf[length] = 0;
    }

    return length;
}

// For when you are dealing with numbers but stupid wchar gets in your way!
s32 numbers_to_utf16(const char* value, s32 value_length, wchar* buf, s32 buf_chars)
{
    const char* ptr = value;
    s32 i = 0;

    for (; *ptr != 0; ptr++)
    {
        wchar add[] = { *ptr, 0 };
        buf[i] = add[0];
        buf[i + 1] = add[1];

        i++;
    }

    return value_length;
}

bool create_velo(ID3D11Texture2D* actual_tex, DXGI_FORMAT actual_format)
{
    bool ret = false;
    MovieProfile& p = movie_profile;

    D2D1_BITMAP_PROPERTIES1 bmp_props = {};
    bmp_props.pixelFormat = D2D1::PixelFormat(actual_format, D2D1_ALPHA_MODE_IGNORE);
    bmp_props.dpiX = 96;
    bmp_props.dpiY = 96;
    bmp_props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    IDXGISurface* dxgi_surface;

    actual_tex->QueryInterface(IID_PPV_ARGS(&dxgi_surface));
    d2d1_context->CreateBitmapFromDxgiSurface(dxgi_surface, &bmp_props, &d2d1_tex);

    dxgi_surface->Release();

    D2D1_COLOR_F color;
    color.r = p.veloc_font_color[0] / 255.0f;
    color.g = p.veloc_font_color[1] / 255.0f;
    color.b = p.veloc_font_color[2] / 255.0f;
    color.a = p.veloc_font_color[3] / 255.0f;
    velo_brush->SetColor(&color);

    #if USE_VELO_V2 || USE_VELO_V3
    D2D1_COLOR_F bord_color;
    bord_color.r = p.veloc_font_border_color[0] / 255.0f;
    bord_color.g = p.veloc_font_border_color[1] / 255.0f;
    bord_color.b = p.veloc_font_border_color[2] / 255.0f;
    bord_color.a = p.veloc_font_border_color[3] / 255.0f;
    velo_border_brush->SetColor(&bord_color);
    #endif

    // DirectWrite uses stupid wchar.

    wchar stupid_buf[128];
    to_utf16(p.veloc_font, strlen(p.veloc_font), stupid_buf, 128);

    HRESULT hr;

    #if USE_VELO_V2
    IDWriteFontCollection* coll = NULL;
    IDWriteFontFamily* font_fam = NULL;
    IDWriteFont* font = NULL;

    dwrite_factory->GetSystemFontCollection(&coll, FALSE);

    UINT font_index;
    BOOL font_exists;
    coll->FindFamilyName(stupid_buf, &font_index, &font_exists);

    if (!font_exists)
    {
        game_log("The specified velo font %s is not installed in the system\n", p.veloc_font);
        goto rfail;
    }

    coll->GetFontFamily(font_index, &font_fam);
    font_fam->GetFirstMatchingFont(p.veloc_font_weight, p.veloc_font_stretch, p.veloc_font_style, &font);
    font->CreateFontFace(&velo_font_face);
    #elif USE_VELO_V1 || USE_VELO_V3
    hr = dwrite_factory->CreateTextFormat(stupid_buf, NULL, p.veloc_font_weight, p.veloc_font_style, p.veloc_font_stretch, p.veloc_font_size, L"en-gb", &velo_text_format);

    // User can have input a font family that doesn't exist.
    if (FAILED(hr))
    {
        game_log("Velocity text could not be created (%#x). Is the %s font installed?\n", hr, p.veloc_font);
        goto rfail;
    }

    velo_text_format->SetTextAlignment(p.veloc_text_align);
    velo_text_format->SetParagraphAlignment(p.veloc_para_align);
    #endif

    #if USE_VELO_V3
    if (velo_geom_cache == NULL && velo_tl_cache == NULL)
    {
        velo_geom_cache = (ID2D1Geometry**)malloc(sizeof(ID2D1Geometry*) * NUM_VELO_CACHES);
        velo_tl_cache = (IDWriteTextLayout**)malloc(sizeof(IDWriteTextLayout*) * NUM_VELO_CACHES);

        memset(velo_geom_cache, 0x00, sizeof(ID2D1Geometry*) * NUM_VELO_CACHES);
        memset(velo_tl_cache, 0x00, sizeof(IDWriteTextLayout*) * NUM_VELO_CACHES);
    }

    for (s32 i = 0; i < NUM_VELO_CACHES; i++)
    {
        svr_maybe_release(&velo_geom_cache[i]);
    }

    for (s32 i = 0; i < NUM_VELO_CACHES; i++)
    {
        svr_maybe_release(&velo_tl_cache[i]);
    }

    memset(velo_geom_cache, 0x00, sizeof(ID2D1Geometry*) * NUM_VELO_CACHES);
    memset(velo_tl_cache, 0x00, sizeof(IDWriteTextLayout*) * NUM_VELO_CACHES);

    D2D1_RECT_F viewbox;
    viewbox.left = 0.0f + p.veloc_padding;
    viewbox.top = 0.0f + p.veloc_padding;
    viewbox.right = movie_width - p.veloc_padding;
    viewbox.bottom = movie_height - p.veloc_padding;

    for (s32 i = 0; i < NUM_VELO_CACHES; i++)
    {
        char buf[64];
        s32 len = stbsp_snprintf(buf, 64, "%d", i);

        wchar stupid_buf[64];
        numbers_to_utf16(buf, len, stupid_buf, 64);

        dwrite_factory->CreateTextLayout(stupid_buf, len, velo_text_format, viewbox.right, viewbox.bottom, &velo_tl_cache[i]);
    }

    for (s32 i = 0; i < NUM_VELO_CACHES; i++)
    {
        IDWriteTextLayout* tl = velo_tl_cache[i];
        tl->Draw(&i, &velo_renderer, viewbox.left, viewbox.top);
    }

    #endif

    ret = true;
    goto rexit;

rfail:
rexit:
    #if USE_VELO_V2
    svr_maybe_release(&font);
    svr_maybe_release(&font_fam);
    svr_maybe_release(&coll);
    #endif

    return ret;
}

bool proc_start(ID3D11Device* d3d11_device, ID3D11DeviceContext* d3d11_context, const char* dest, const char* profile, ID3D11ShaderResourceView* game_content_srv)
{
    bool ret = false;

    HRESULT hr;

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

    // If this doesn't work then we use the default profile in code.
    if (!read_profile(full_profile_path, &movie_profile))
    {
        svr_log("Could not load profile %s, setting default\n", profile);
        set_default_profile(&movie_profile);
    }

    else
    {
        if (!verify_profile(&movie_profile))
        {
            svr_log("Could not verify profile, setting default\n");
            set_default_profile(&movie_profile);
        }
    }

    assert(verify_profile(&movie_profile));

    ID3D11Resource* content_tex_res;
    game_content_srv->GetResource(&content_tex_res);

    ID3D11Texture2D* content_tex;
    content_tex_res->QueryInterface(IID_PPV_ARGS(&content_tex));

    D3D11_TEXTURE2D_DESC tex_desc;
    content_tex->GetDesc(&tex_desc);

    content_tex_res->Release();
    content_tex->Release();

    frame_num = 0;
    movie_width = tex_desc.Width;
    movie_height = tex_desc.Height;
    StringCchCopyA(movie_path, MAX_PATH, dest);

    if (!start_with_sw(d3d11_device))
    {
        goto rfail;
    }

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
    }

    if (movie_profile.veloc_enabled)
    {
        // No mosample means the game texture is used directly.
        ID3D11Texture2D* actual_tex = content_tex;
        DXGI_FORMAT actual_format = tex_desc.Format;

        if (movie_profile.mosample_enabled)
        {
            actual_tex = work_tex;
            actual_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }

        if (!create_velo(actual_tex, actual_format))
        {
            goto rfail;
        }
    }

    // Need to overwrite with new data.
    ffmpeg_read_queue.reset();

    // Each buffer contains 1 uncompressed frame.

    for (s32 i = 0; i < MAX_BUFFERED_SEND_BUFS; i++)
    {
        ThreadPipeData pipe_data;
        pipe_data.ptr = (u8*)malloc(pxconv_total_plane_sizes);
        pipe_data.size = pxconv_total_plane_sizes;

        ffmpeg_send_bufs[i] = pipe_data;
    }

    for (s32 i = 0; i < MAX_BUFFERED_SEND_BUFS; i++)
    {
        ffmpeg_read_queue.push(&ffmpeg_send_bufs[i]);
    }

    if (movie_profile.mosample_enabled)
    {
        mosample_remainder = 0.0f;

        s32 sps = movie_profile.movie_fps * movie_profile.mosample_mult;
        mosample_remainder_step = (1.0f / sps) / (1.0f / movie_profile.movie_fps);
    }

    if (!start_ffmpeg_proc())
    {
        goto rfail;
    }

    game_log("Starting movie to %s\n", dest);

    log_profile(&movie_profile);

    ret = true;
    goto rexit;

rfail:
    free_all_dynamic_proc_stuff();

rexit:
    return ret;
}

// For SW encoding, send uncompressed frame over pipe.
void send_converted_video_frame_to_ffmpeg(ID3D11DeviceContext* d3d11_context)
{
    svr_sem_wait(&ffmpeg_read_sem);

    ThreadPipeData pipe_data;
    bool res1 = ffmpeg_read_queue.pull(&pipe_data);
    assert(res1);

    svr_start_prof(&dl_prof);
    download_textures(d3d11_context, pxconv_texs, pxconv_dls, used_pxconv_planes, pipe_data.ptr, pipe_data.size);
    svr_end_prof(&dl_prof);

    ffmpeg_write_queue.push(&pipe_data);

    svr_sem_release(&ffmpeg_write_sem);
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

    ID3D11ShaderResourceView* null_srvs[] = { NULL };
    ID3D11UnorderedAccessView* null_uavs[] = { NULL };

    d3d11_context->CSSetShaderResources(0, 1, null_srvs);
    d3d11_context->CSSetUnorderedAccessViews(0, 1, null_uavs, NULL);
}

void encode_video_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* srv)
{
    if (movie_profile.veloc_enabled)
    {
        float p = movie_profile.veloc_padding;

        // We only deal with XY velo.
        float vel = sqrt(player_velo[0] * player_velo[0] + player_velo[1] * player_velo[1]);
        s32 real_vel = (s32)(vel + 0.5f);

        char buf[64];
        s32 len = stbsp_snprintf(buf, 64, "%d", real_vel);

        #if USE_VELO_V2
        UINT32* code_points = (UINT32*)_alloca(sizeof(UINT32) * len);
        UINT16* glyph_idxs = (UINT16*)_alloca(sizeof(UINT16) * len);

        for (s32 i = 0; i < len; i++)
        {
            code_points[i] = buf[i];
        }

        velo_font_face->GetGlyphIndicesW(code_points, len, glyph_idxs);

        // In epic D2D1 fashion everything has to be remade every frame!

        ID2D1PathGeometry* geom;
        d2d1_factory->CreatePathGeometry(&geom);

        ID2D1GeometrySink* sink;
        geom->Open(&sink);
        velo_font_face->GetGlyphRunOutline(movie_profile.veloc_font_size, glyph_idxs, NULL, NULL, len, FALSE, FALSE, sink);
        sink->Close();

        // The geometry has its origin in the bottom left for who knows what reason!
        // We want to use the center of the text for alignment because that's what human beings want.

        D2D1_RECT_F geom_rect;
        geom->GetBounds(NULL, &geom_rect);

        float geom_width = geom_rect.right - geom_rect.left;
        float geom_height = geom_rect.bottom - geom_rect.top;

        D2D1_MATRIX_3X2_F transform = {};
        transform.m11 = 1.0f;
        transform.m22 = 1.0f;
        transform.dx -= geom_width / 2.0f;
        transform.dy += geom_height / 2.0f;
        transform.dx += movie_width / 2.0f;
        transform.dy += movie_height / 2.0f;

        d2d1_context->SetTarget(d2d1_tex);
        d2d1_context->BeginDraw();
        d2d1_context->SetTransform(transform);
        d2d1_context->FillGeometry(geom, velo_brush, NULL);

        if (movie_profile.veloc_font_border_size > 0)
        {
            d2d1_context->DrawGeometry(geom, velo_border_brush, movie_profile.veloc_font_border_size, NULL);
        }

        d2d1_context->DrawRectangle(&geom_rect, velo_brush, 1.0f);

        d2d1_context->EndDraw();
        d2d1_context->SetTarget(NULL);

        sink->Release();
        geom->Release();
        #elif USE_VELO_V1
        D2D1_RECT_F viewbox;
        viewbox.left = 0.0f + p;
        viewbox.top = 0.0f + p;
        viewbox.right = movie_width - p;
        viewbox.bottom = movie_height - p;

        wchar stupid_buf[64];
        s32 stupid_len = numbers_to_utf16(buf, len, stupid_buf, 64);

        d2d1_context->SetTarget(d2d1_tex);
        d2d1_context->BeginDraw();
        d2d1_context->DrawTextW(stupid_buf, stupid_len, velo_text_format, &viewbox, velo_brush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_GDI_NATURAL);
        d2d1_context->EndDraw();
        d2d1_context->SetTarget(NULL);
        #elif USE_VELO_V3
        D2D1_RECT_F viewbox;
        viewbox.left = 0.0f + p;
        viewbox.top = 0.0f + p;
        viewbox.right = movie_width - p;
        viewbox.bottom = movie_height - p;

        IDWriteTextLayout* tl = velo_tl_cache[real_vel];

        d2d1_context->SetTarget(d2d1_tex);
        d2d1_context->BeginDraw();
        tl->Draw(&real_vel, &velo_renderer, viewbox.left, viewbox.top);
        d2d1_context->EndDraw();
        d2d1_context->SetTarget(NULL);
        #endif
    }

    convert_pixel_formats(d3d11_context, srv);
    send_converted_video_frame_to_ffmpeg(d3d11_context);
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

        encode_video_frame(d3d11_context, work_tex_srv);

        mosample_remainder -= 1.0f;

        s32 additional = mosample_remainder;

        if (additional > 0)
        {
            for (s32 i = 0; i < additional; i++)
            {
                encode_video_frame(d3d11_context, work_tex_srv);
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

void proc_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv)
{
    if (frame_num > 0)
    {
        svr_start_prof(&frame_prof);

        if (movie_profile.mosample_enabled)
        {
            mosample_game_frame(d3d11_context, game_content_srv);
        }

        else
        {
            encode_video_frame(d3d11_context, game_content_srv);
        }

        svr_end_prof(&frame_prof);
    }

    frame_num++;
}

void proc_give_velocity(float* xyz)
{
    memcpy(player_velo, xyz, sizeof(float) * 3);
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
    game_log("Ending movie\n");

    end_ffmpeg_proc();

    free_all_dynamic_sw_stuff();
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

    game_log("Movie finished\n");
}

s32 proc_get_game_rate()
{
    if (movie_profile.mosample_enabled)
    {
        return movie_profile.movie_fps * movie_profile.mosample_mult;
    }

    return movie_profile.movie_fps;
}
