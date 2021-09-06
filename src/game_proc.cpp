#include "game_proc.h"
#include "game_shared.h"
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <strsafe.h>
#include <dwrite.h>
#include <d2d1.h>
#include <malloc.h>
#include <assert.h>
#include <limits>
#include "svr_prof.h"
#include "svr_stream.h"
#include "svr_sem.h"
#include <nvEncodeAPI.h>
#include "game_proc_profile.h"

// Don't use fatal process ending errors in here as this is used both in standalone and in integration.
// It is only standalone SVR that can do fatal processs ending errors.

// Must be a power of 2.
const s32 NUM_BUFFERED_DL_TEXS = 2;

// Rotation of CPU textures to download to.
struct CpuTexDl
{
    // Keep the textures rotating for downloads.
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

ID3D11Texture2D* work_tex;
ID3D11RenderTargetView* work_tex_rtv;
ID3D11ShaderResourceView* work_tex_srv;
ID3D11UnorderedAccessView* work_tex_uav;

// For when we don't have typed UAV loads and stores we have to use buffers
// instead of textures. So this buffer contains the equivalent 32 bit floats for R32G32B32A32.
// Because it is not a texture, it needs an additional constant buffer that tells the width so we can calculate the index.
ID3D11Buffer* work_buf_legacy_sb;
ID3D11ShaderResourceView* work_buf_legacy_sb_srv;
ID3D11UnorderedAccessView* work_buf_legacy_sb_uav;

// -------------------------------------------------
// Mosample state.

__declspec(align(16)) struct MosampleCb
{
    float mosample_weight;
};

__declspec(align(16)) struct MosampleLegacyCb
{
    UINT dest_texture_width;
};

ID3D11ComputeShader* mosample_cs;

// Constains the mosample weight and for legacy systems it also contains the work buffer width.
ID3D11Buffer* mosample_cb;
ID3D11Buffer* mosample_legacy_cb;

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
// HW caps.

// If the hardware does not have this support, then buffers need to be used instead of textures.
bool hw_has_typed_uav_support;

// NVENC support will always be used if available (not an option).
bool hw_has_nvenc_support;

// Certain things are locked behind HW models.
bool hw_nvenc_has_lookahead;
bool hw_nvenc_has_psycho_aq;
bool hw_nvenc_has_lossless_encode;
bool hw_nvenc_has_444_encode;

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
// For NVENC encoding these buffers are some amount of compressed packets in a H264 stream.
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

    HRESULT hr;

    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        PxConvText& text = PXCONV_INI_TEXT_TABLE[i];

        if (!strcmp(text.format, movie_profile.sw_pxformat) && !strcmp(text.color_space, movie_profile.sw_colorspace))
        {
            movie_pxconv = (PxConv)i;
            break;
        }
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
// NVENC encoding.

// NVENC encoding for NVIDIA cards.
// Implemented with version 11.0.10 of the SDK.
// See the documentation in NVENC_VideoEncoder_API_ProgGuide.pdf (from NVIDIA website).

// This device and context are used in the NVENC thread that receives the encoded output.
// We pass this to NVENC for its processing for multithreaded purposes since ID3D11DeviceContext is not thread safe (so we cannot
// use the existing svr_d3d11_device).

ID3D11Device* nvenc_d3d11_device;
ID3D11DeviceContext* nvenc_d3d11_context;
NV_ENCODE_API_FUNCTION_LIST nvenc_funs;

// NVENC resources have to be destroyed in a specific order and all pending operations need to be flushed.

void* nvenc_encoder;
HANDLE nvenc_thread;

SvrSemaphore nvenc_raw_sem;
SvrSemaphore nvenc_enc_sem;

struct NvencPicture
{
    ID3D11Texture2D* tex;
    NV_ENC_REGISTERED_PTR resource;
    NV_ENC_INPUT_PTR mapped_resource;
};

s32 nvenc_num_pics;
NvencPicture* nvenc_pics;

#define NV_FAILED(ns) (((s32)(ns)) != NV_ENC_SUCCESS)

// Stuff that is created during init.
void free_all_static_nvenc_stuff()
{
    svr_maybe_release(&nvenc_d3d11_device);
    svr_maybe_release(&nvenc_d3d11_context);
    nvenc_funs = {};
}

// Stuff that is created during movie start.
void free_all_dynamic_nvenc_stuff()
{
    if (nvenc_pics)
    {
        for (s32 i = 0; i < nvenc_num_pics; i++)
        {
            NvencPicture* pic = &nvenc_pics[i];

            svr_maybe_release(&pic->tex);
        }

        free(nvenc_pics);
        nvenc_pics = NULL;
    }

    nvenc_num_pics = 0;

    if (nvenc_encoder)
    {
        nvenc_funs.nvEncDestroyEncoder(nvenc_encoder);
        nvenc_encoder = NULL;
    }
}

s32 check_nvenc_support_cap(NV_ENC_CAPS cap)
{
    assert(nvenc_encoder);

    NV_ENC_CAPS_PARAM param = {};
    param.version = NV_ENC_CAPS_PARAM_VER;
    param.capsToQuery = cap;

    int v;
    nvenc_funs.nvEncGetEncodeCaps(nvenc_encoder, NV_ENC_CODEC_H264_GUID, &param, &v);

    return v;
}

bool init_nvenc()
{
    bool ret = false;

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS nvenc_open_session_params = {};

    HRESULT hr;
    D3D_FEATURE_LEVEL created_device_level;

    UINT device_create_flags = 0;

    NVENCSTATUS ns;

    // Should be good enough for all the features that we make use of.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_VERSION = D3D_FEATURE_LEVEL_11_0;

    const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
        MINIMUM_DEVICE_VERSION
    };

    #if SVR_DEBUG
    device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &nvenc_d3d11_device, &created_device_level, &nvenc_d3d11_context);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create D3D11 device for NVENC encoding (%#x)\n", hr);
        goto rfail;
    }

    nvenc_funs.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NvEncodeAPICreateInstance(&nvenc_funs);

    nvenc_open_session_params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    nvenc_open_session_params.device = nvenc_d3d11_device;
    nvenc_open_session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    nvenc_open_session_params.apiVersion = NVENCAPI_VERSION;
    ns = nvenc_funs.nvEncOpenEncodeSessionEx(&nvenc_open_session_params, &nvenc_encoder);

    if (NV_FAILED(ns))
    {
        svr_log("ERROR: Could not open NVENC encoder (%d)\n", (s32)ns);
        goto rfail;
    }

    hw_nvenc_has_lookahead = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_LOOKAHEAD);
    hw_nvenc_has_psycho_aq = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ);
    hw_nvenc_has_lossless_encode = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE);
    hw_nvenc_has_444_encode = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE);

    svr_log("NVENC lookahead support: %d\n", (s32)hw_nvenc_has_lookahead);
    svr_log("NVENC psycho aq support: %d\n", (s32)hw_nvenc_has_psycho_aq);
    svr_log("NVENC lossless encode support: %d\n", (s32)hw_nvenc_has_lossless_encode);
    svr_log("NVENC 444 encode support: %d\n", (s32)hw_nvenc_has_444_encode);

    ret = true;
    goto rexit;

rfail:
    free_all_static_nvenc_stuff();

rexit:
    return ret;
}

DWORD WINAPI nvenc_thread_proc(LPVOID lpParameter)
{
    return 0;
}

bool start_with_nvenc()
{
    bool ret = false;

    NVENCSTATUS ns;
    HRESULT hr;

    GUID preset_guid = NV_ENC_PRESET_LOSSLESS_HP_GUID;

    NV_ENC_INITIALIZE_PARAMS nvenc_init_params = {};
    NV_ENC_PRESET_CONFIG preset_config;

    nvenc_init_params.version = NV_ENC_INITIALIZE_PARAMS_VER;
    nvenc_init_params.encodeGUID = NV_ENC_CODEC_H264_GUID;
    nvenc_init_params.presetGUID = preset_guid;
    nvenc_init_params.encodeWidth = movie_width;
    nvenc_init_params.encodeHeight = movie_height;
    nvenc_init_params.frameRateNum = movie_profile.movie_fps;
    nvenc_init_params.frameRateDen = 1;
    nvenc_init_params.enableEncodeAsync = 1;
    nvenc_init_params.enablePTD = 1;
    ns = nvenc_funs.nvEncInitializeEncoder(nvenc_encoder, &nvenc_init_params);

    if (NV_FAILED(ns))
    {
        svr_log("ERROR: Could not initialize the NVENC encoder (%d)\n", (s32)ns);
        goto rfail;
    }

    preset_config.version = NV_ENC_PRESET_CONFIG_VER;
    preset_config.presetCfg.version = NV_ENC_CONFIG_VER;
    nvenc_funs.nvEncGetEncodePresetConfig(nvenc_encoder, NV_ENC_CODEC_H264_GUID, preset_guid, &preset_config);

    nvenc_num_pics = svr_max(4, preset_config.presetCfg.frameIntervalP * 2 * 2);
    nvenc_pics = (NvencPicture*)malloc(sizeof(NvencPicture) * nvenc_num_pics);
    memset(nvenc_pics, 0x00, sizeof(NvencPicture) * nvenc_num_pics);

    for (s32 i = 0; i < nvenc_num_pics; i++)
    {
        D3D11_TEXTURE2D_DESC nvenc_tex_desc = {};
        NV_ENC_REGISTER_RESOURCE nvenc_resource = {};

        nvenc_tex_desc.Width = movie_width;
        nvenc_tex_desc.Height = movie_height;
        nvenc_tex_desc.MipLevels = 1;
        nvenc_tex_desc.ArraySize = 1;
        nvenc_tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        nvenc_tex_desc.SampleDesc.Count = 1;
        nvenc_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        nvenc_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        ID3D11Texture2D* nvenc_tex = NULL;
        hr = nvenc_d3d11_device->CreateTexture2D(&nvenc_tex_desc, NULL, &nvenc_tex);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create NVENC texture (%#x)\n", hr);
            goto rfail;
        }

        nvenc_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

        nvenc_resource.version = NV_ENC_REGISTER_RESOURCE_VER;
        nvenc_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        nvenc_resource.width = movie_width;
        nvenc_resource.height = movie_height;
        nvenc_resource.resourceToRegister = nvenc_tex;
        nvenc_resource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
        nvenc_resource.bufferUsage = NV_ENC_INPUT_IMAGE;

        ns = nvenc_funs.nvEncRegisterResource(nvenc_encoder, &nvenc_resource);

        if (NV_FAILED(ns))
        {
            svr_log("ERROR: Could not register NVENC resource (%d)\n", (s32)ns);
            goto rfail;
        }

        NvencPicture* pic = &nvenc_pics[i];
        pic->tex = nvenc_tex;
        pic->resource = nvenc_resource.registeredResource;
    }

    ret = true;
    goto rexit;

rfail:
    free_all_dynamic_nvenc_stuff();

rexit:
    return ret;
}

// NV_ENC_CAPS_SUPPORT_YUV444_ENCODE
// NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE

const UINT NVIDIA_VENDOR_ID = 0x10de;

// Blocked adapters from OBS.
// See https://github.com/obsproject/obs-studio/blob/master/plugins/obs-ffmpeg/obs-ffmpeg.c
// Use https://www.pcilookup.com/ to see more information about device and vendor ids.
const UINT BLACKLISTED_ADAPTERS[] = {
    0x1298, // GK208M [GeForce GT 720M]
    0x1140, // GF117M [GeForce 610M/710M/810M/820M / GT 620M/625M/630M/720M]
    0x1293, // GK208M [GeForce GT 730M]
    0x1290, // GK208M [GeForce GT 730M]
    0x0fe1, // GK107M [GeForce GT 730M]
    0x0fdf, // GK107M [GeForce GT 740M]
    0x1294, // GK208M [GeForce GT 740M]
    0x1292, // GK208M [GeForce GT 740M]
    0x0fe2, // GK107M [GeForce GT 745M]
    0x0fe3, // GK107M [GeForce GT 745M]
    0x1140, // GF117M [GeForce 610M/710M/810M/820M / GT 620M/625M/630M/720M]
    0x0fed, // GK107M [GeForce 820M]
    0x1340, // GM108M [GeForce 830M]
    0x1393, // GM107M [GeForce 840M]
    0x1341, // GM108M [GeForce 840M]
    0x1398, // GM107M [GeForce 845M]
    0x1390, // GM107M [GeForce 845M]
    0x1344, // GM108M [GeForce 845M]
    0x1299, // GK208BM [GeForce 920M]
    0x134f, // GM108M [GeForce 920MX]
    0x134e, // GM108M [GeForce 930MX]
    0x1349, // GM108M [GeForce 930M]
    0x1346, // GM108M [GeForce 930M]
    0x179c, // GM107 [GeForce 940MX]
    0x139c, // GM107M [GeForce 940M]
    0x1347, // GM108M [GeForce 940M]
    0x134d, // GM108M [GeForce 940MX]
    0x134b, // GM108M [GeForce 940MX]
    0x1399, // GM107M [GeForce 945M]
    0x1348, // GM108M [GeForce 945M / 945A]
    0x1d01, // GP108 [GeForce GT 1030]
    0x0fc5, // GK107 [GeForce GT 1030]
    0x174e, // GM108M [GeForce MX110]
    0x174d, // GM108M [GeForce MX130]
    0x1d10, // GP108M [GeForce MX150]
    0x1d12, // GP108M [GeForce MX150]
    0x1d11, // GP108M [GeForce MX230]
    0x1d13, // GP108M [GeForce MX250]
    0x1d52, // GP108BM [GeForce MX250]
    0x1c94, // GP107 [GeForce MX350]
    0x137b, // GM108GLM [Quadro M520 Mobile]
    0x1d33, // GP108GLM [Quadro P500 Mobile]
    0x137a, // GM108GLM [Quadro K620M / Quadro M500M]
};

bool is_blacklisted_device(UINT device_id)
{
    for (s32 i = 0; i < SVR_ARRAY_SIZE(BLACKLISTED_ADAPTERS); i++)
    {
        if (device_id == BLACKLISTED_ADAPTERS[i])
        {
            return true;
        }
    }

    return false;
}

bool has_nvidia_device()
{
    IDXGIFactory2* dxgi_factory;

    IDXGIAdapter1* adapter = NULL;
    bool supported = false;

    CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi_factory));

    for (UINT i = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i, &adapter)); i++)
    {
        DXGI_ADAPTER_DESC1 adapter_desc;
        adapter->GetDesc1(&adapter_desc);

        adapter->Release();

        // Integrated chips are not supported.
        if (adapter_desc.VendorId == NVIDIA_VENDOR_ID && !is_blacklisted_device(adapter_desc.DeviceId))
        {
            supported = true;
            break;
        }
    }

    dxgi_factory->Release();

    return supported;
}

bool has_nvenc_driver()
{
    HMODULE dll = LoadLibraryA("nvEncodeAPI.dll");
    bool exists = dll != NULL;
    FreeLibrary(dll);
    return exists;
}

bool is_nvenc_supported_real()
{
    if (!has_nvidia_device())
    {
        return false;
    }

    if (!has_nvenc_driver())
    {
        return false;
    }

    return true;
}

bool proc_is_nvenc_supported()
{
    // Store the result because this will probably be called multiple times, and the result will not change.
    static bool ret = is_nvenc_supported_real();
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
        // For NVENC encoding we send smaller data so it's faster.

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

    if (!hw_has_nvenc_support)
    {
        for (s32 i = 0; i < NUM_PXCONVS; i++)
        {
            if (!create_a_cs_shader(PXCONV_SHADER_NAMES[i], file_mem, SHADER_MEM_SIZE, d3d11_device, &pxconv_cs[i])) goto rfail;
        }
    }

    if (hw_has_typed_uav_support)
    {
        if (!create_a_cs_shader("c52620855f15b2c47b8ca24b890850a90fdc7017", file_mem, SHADER_MEM_SIZE, d3d11_device, &mosample_cs)) goto rfail;
    }

    else
    {
        if (!create_a_cs_shader("cf3aa43b232f4624ef5e002a716b67045f45b044", file_mem, SHADER_MEM_SIZE, d3d11_device, &mosample_cs)) goto rfail;
    }

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
    svr_maybe_release(&mosample_legacy_cb);
}

void free_all_dynamic_proc_stuff()
{
    svr_maybe_release(&work_tex);
    svr_maybe_release(&work_tex_rtv);
    svr_maybe_release(&work_tex_srv);
    svr_maybe_release(&work_tex_uav);

    svr_maybe_release(&work_buf_legacy_sb);
    svr_maybe_release(&work_buf_legacy_sb_srv);
    svr_maybe_release(&work_buf_legacy_sb_uav);
}

bool proc_init(const char* svr_path, ID3D11Device* d3d11_device)
{
    bool ret = false;

    StringCchCopyA(svr_resource_path, MAX_PATH, svr_path);

    // See if typed UAV loads and stores are supported so we can decide what code path to use.
    // It becomes more complicated without this, but still doable. More reports than expected have come out regarding the absence
    // of this feature in certain hardware.

    // If we have hardware support for this, we can add the game content directly to the work texture and be on our way.
    // If we don't we have to create a structured buffer that we can motion sample on instead.
    // Both ways will be identical in memory but hardware support differs on how efficiently it can be worked on.

    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fmt_support2;
    fmt_support2.InFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &fmt_support2, sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT2));

    hw_has_typed_uav_support = (fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) && (fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE);

    hw_has_nvenc_support = proc_is_nvenc_supported();

    IDXGIDevice* dxgi_device;
    d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    IDXGIAdapter* dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);

    DXGI_ADAPTER_DESC dxgi_adapter_desc;
    dxgi_adapter->GetDesc(&dxgi_adapter_desc);

    dxgi_adapter->Release();
    dxgi_device->Release();

    // Useful for future troubleshooting.
    // Use https://www.pcilookup.com/ to see more information about device and vendor ids.
    svr_log("Using graphics device %x by vendor %x\n", dxgi_adapter_desc.DeviceId, dxgi_adapter_desc.VendorId);

    svr_log("Typed UAV support: %d\n", (s32)hw_has_typed_uav_support);
    svr_log("NVENC support: %d\n", (s32)hw_has_nvenc_support);

    // Minimum size of a constant buffer is 16 bytes.

    // For mosample_buffer_0.
    D3D11_BUFFER_DESC mosample_cb_desc = {};
    mosample_cb_desc.ByteWidth = sizeof(MosampleCb);
    mosample_cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    mosample_cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mosample_cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mosample_cb_desc.MiscFlags = 0;

    HRESULT hr;
    hr = d3d11_device->CreateBuffer(&mosample_cb_desc, NULL, &mosample_cb);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create mosample constant buffer (%#x)\n", hr);
        goto rfail;
    }

    if (!hw_has_typed_uav_support)
    {
        mosample_cb_desc.ByteWidth = sizeof(MosampleLegacyCb);

        hr = d3d11_device->CreateBuffer(&mosample_cb_desc, NULL, &mosample_legacy_cb);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create legacy mosample constant buffer (%#x)\n", hr);
            goto rfail;
        }
    }

    if (!create_shaders(d3d11_device))
    {
        goto rfail;
    }

    svr_sem_init(&ffmpeg_write_sem, 0, MAX_BUFFERED_SEND_BUFS);
    svr_sem_init(&ffmpeg_read_sem, MAX_BUFFERED_SEND_BUFS, MAX_BUFFERED_SEND_BUFS);

    ffmpeg_write_queue.init(MAX_BUFFERED_SEND_BUFS);
    ffmpeg_read_queue.init(MAX_BUFFERED_SEND_BUFS);

    ffmpeg_thread = CreateThread(NULL, 0, ffmpeg_thread_proc, NULL, 0, NULL);

    if (hw_has_nvenc_support)
    {
        if (!init_nvenc())
        {
            goto rfail;
        }
    }

    ret = true;
    goto rexit;

rfail:
    free_all_static_proc_stuff();

rexit:
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

    if (hw_has_nvenc_support)
    {
        // We are sending an H264 stream.
        StringCchCatA(full_args, full_args_size, " -f rawvideo -vcodec h264");
    }

    else
    {
        PxConvText pxconv_text = PXCONV_FFMPEG_TEXT_TABLE[movie_pxconv];

        // We are sending uncompressed frames.
        StringCchCatA(full_args, full_args_size, " -f rawvideo -vcodec rawvideo");

        // Pixel format that goes through the pipe.
        StringCchPrintfA(buf, ARGS_BUF_SIZE, " -pix_fmt %s", pxconv_text.format);
        StringCchCatA(full_args, full_args_size, buf);
    }

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

    if (hw_has_nvenc_support)
    {
        // Output video codec.
        StringCchCatA(full_args, full_args_size, " -vcodec h264");
    }

    else
    {
        PxConvText pxconv_text = PXCONV_FFMPEG_TEXT_TABLE[movie_pxconv];

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
// For NVENC encoding we send compressed H264 packets which are then muxed in the ffmpeg process.
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

    if (hw_has_nvenc_support)
    {
        if (!start_with_nvenc())
        {
            goto rfail;
        }
    }

    else
    {
        if (!start_with_sw(d3d11_device))
        {
            goto rfail;
        }
    }

    if (hw_has_typed_uav_support)
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

    else
    {
        D3D11_BUFFER_DESC work_buf_desc = {};
        work_buf_desc.ByteWidth = calc_bytes_pitch(DXGI_FORMAT_R32G32B32A32_FLOAT) * tex_desc.Width * tex_desc.Height;
        work_buf_desc.Usage = D3D11_USAGE_DEFAULT;
        work_buf_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        work_buf_desc.CPUAccessFlags = 0;
        work_buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        work_buf_desc.StructureByteStride = calc_bytes_pitch(DXGI_FORMAT_R32G32B32A32_FLOAT);

        hr = d3d11_device->CreateBuffer(&work_buf_desc, NULL, &work_buf_legacy_sb);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create legacy work buffer (%#x)\n", hr);
            goto rfail;
        }

        d3d11_device->CreateShaderResourceView(work_buf_legacy_sb, NULL, &work_buf_legacy_sb_srv);
        d3d11_device->CreateUnorderedAccessView(work_buf_legacy_sb, NULL, &work_buf_legacy_sb_uav);

        MosampleLegacyCb cb_data;
        cb_data.dest_texture_width = tex_desc.Width;

        // For dest_texture_width.
        update_constant_buffer(d3d11_context, mosample_legacy_cb, &cb_data, sizeof(MosampleLegacyCb));
    }

    // Need to overwrite with new data.
    ffmpeg_read_queue.reset();

    if (hw_has_nvenc_support)
    {
        assert(false);

        for (s32 i = 0; i < MAX_BUFFERED_SEND_BUFS; i++)
        {
            // Can we preallocate anything with NVENC encoding? The size of the H264 stream packets will wary but there still has
            // to be an upper limit.
            ffmpeg_send_bufs[i] = {};
        }
    }

    else
    {
        // Each buffer contains 1 uncompressed frame.

        for (s32 i = 0; i < MAX_BUFFERED_SEND_BUFS; i++)
        {
            ThreadPipeData pipe_data;
            pipe_data.ptr = (u8*)malloc(pxconv_total_plane_sizes);
            pipe_data.size = pxconv_total_plane_sizes;

            ffmpeg_send_bufs[i] = pipe_data;
        }
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

    if (hw_has_typed_uav_support)
    {
        download_textures(d3d11_context, pxconv_texs, pxconv_dls, used_pxconv_planes, pipe_data.ptr, pipe_data.size);
    }

    else
    {
        assert(false);
    }

    svr_end_prof(&dl_prof);

    ffmpeg_write_queue.push(&pipe_data);

    svr_sem_release(&ffmpeg_write_sem);
}

void encode_game_frame_with_nvenc(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* srv)
{
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

    if (hw_has_typed_uav_support)
    {
        d3d11_context->CSSetConstantBuffers(0, 1, &mosample_cb);
        d3d11_context->CSSetUnorderedAccessViews(0, 1, &work_tex_uav, NULL);
    }

    else
    {
        ID3D11Buffer* cbs[] = { mosample_cb, mosample_legacy_cb };
        d3d11_context->CSSetConstantBuffers(0, 2, cbs);
        d3d11_context->CSSetUnorderedAccessViews(0, 1, &work_buf_legacy_sb_uav, NULL);
    }

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
    if (hw_has_nvenc_support)
    {
        encode_game_frame_with_nvenc(d3d11_context, srv);

        // The compressed H264 stream is sent to the ffmpeg process elsewhere.
        // We will receive a notification from NVENC when it's ready.
    }

    else
    {
        convert_pixel_formats(d3d11_context, srv);
        send_converted_video_frame_to_ffmpeg(d3d11_context);
    }
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
    float x = xyz[0];
    float y = xyz[1];
    float z = xyz[2];
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

    if (hw_has_nvenc_support)
    {
        free_all_dynamic_nvenc_stuff();
    }

    else
    {
        free_all_dynamic_sw_stuff();
    }

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
