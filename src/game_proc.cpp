#include "game_proc.h"
#include "game_shared.h"
#include "svr_api.h"
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include "svr_ini.h"
#include <strsafe.h>
#include <dwrite.h>
#include <d2d1.h>
#include <malloc.h>
#include <assert.h>
#include <limits>
#include "svr_prof.h"
#include "svr_atom.h"
#include "svr_stream.h"
#include "svr_sem.h"
#include <nvEncodeAPI.h>

struct StrIntMapping
{
    const char* name;
    s32 value;
};

// Font stuff for velo.

// Names for ini.
StrIntMapping FONT_WEIGHT_TABLE[] = {
    StrIntMapping { "thin", DWRITE_FONT_WEIGHT_THIN },
    StrIntMapping { "extralight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT },
    StrIntMapping { "light", DWRITE_FONT_WEIGHT_LIGHT },
    StrIntMapping { "semilight", DWRITE_FONT_WEIGHT_SEMI_LIGHT },
    StrIntMapping { "normal", DWRITE_FONT_WEIGHT_NORMAL },
    StrIntMapping { "medium", DWRITE_FONT_WEIGHT_MEDIUM },
    StrIntMapping { "semibold", DWRITE_FONT_WEIGHT_SEMI_BOLD },
    StrIntMapping { "bold", DWRITE_FONT_WEIGHT_BOLD },
    StrIntMapping { "extrabold", DWRITE_FONT_WEIGHT_EXTRA_BOLD },
    StrIntMapping { "black", DWRITE_FONT_WEIGHT_BLACK },
    StrIntMapping { "extrablack", DWRITE_FONT_WEIGHT_EXTRA_BLACK },
};

// Names for ini.
StrIntMapping FONT_STYLE_TABLE[] = {
    StrIntMapping { "normal", DWRITE_FONT_STYLE_NORMAL },
    StrIntMapping { "oblique", DWRITE_FONT_STYLE_OBLIQUE },
    StrIntMapping { "italic", DWRITE_FONT_STYLE_ITALIC },
};

// Names for ini.
StrIntMapping FONT_STRETCH_TABLE[] = {
    StrIntMapping { "undefined", DWRITE_FONT_STRETCH_UNDEFINED },
    StrIntMapping { "ultracondensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED },
    StrIntMapping { "extracondensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED },
    StrIntMapping { "condensed", DWRITE_FONT_STRETCH_CONDENSED },
    StrIntMapping { "semicondensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED },
    StrIntMapping { "normal", DWRITE_FONT_STRETCH_NORMAL },
    StrIntMapping { "semiexpanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED },
    StrIntMapping { "expanded", DWRITE_FONT_STRETCH_EXPANDED },
    StrIntMapping { "extraexpanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED },
    StrIntMapping { "ultraexpanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED },
};

// Names for ini.
StrIntMapping TEXT_ALIGN_TABLE[] = {
    StrIntMapping { "leading", DWRITE_TEXT_ALIGNMENT_LEADING },
    StrIntMapping { "trailing", DWRITE_TEXT_ALIGNMENT_TRAILING },
    StrIntMapping { "center", DWRITE_TEXT_ALIGNMENT_CENTER },
};

// Names for ini.
StrIntMapping PARAGRAPH_ALIGN_TABLE[] = {
    StrIntMapping { "near", DWRITE_PARAGRAPH_ALIGNMENT_NEAR },
    StrIntMapping { "far", DWRITE_PARAGRAPH_ALIGNMENT_FAR },
    StrIntMapping { "center", DWRITE_PARAGRAPH_ALIGNMENT_CENTER },
};

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

ID3D11ComputeShader* mosample_cs;
ID3D11ComputeShader* mosample_legacy_cs;

// Both these constant buffers should be merged into one instead and the shader preprocessor selects what data to have.
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

const s32 MAX_VELOC_FONT_NAME = 128;

struct MovieProfile
{
    s32 movie_fps;

    // For SW encoding:
    const char* sw_encoder;
    const char* sw_pxformat;
    const char* sw_colorspace;
    s32 sw_crf;
    const char* sw_x264_preset;
    s32 sw_x264_intra;

    // For NVENC encoding:
    // Nothing yet.

    s32 mosample_enabled;
    s32 mosample_mult;
    float mosample_exposure;

    s32 veloc_enabled;
    char veloc_font[MAX_VELOC_FONT_NAME];
    s32 veloc_font_size;
    s32 veloc_font_color[4];
    DWRITE_FONT_STYLE veloc_font_style;
    DWRITE_FONT_WEIGHT veloc_font_weight;
    DWRITE_FONT_STRETCH veloc_font_stretch;
    DWRITE_TEXT_ALIGNMENT veloc_text_align;
    DWRITE_PARAGRAPH_ALIGNMENT veloc_para_align;
    s32 veloc_padding;
};

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

// We write data to the ffmpeg process through this pipe.
// It is redirected to their stdin.
HANDLE ffmpeg_write_pipe;

HANDLE ffmpeg_proc;

// How many completed (uncompressed) frames we keep in memory waiting to be sent to ffmpeg.
const s32 MAX_BUFFERED_SEND_FRAMES = 8;

// The buffers that are sent to the ffmpeg process.
// Size of each buffer depends on the type of stream being sent, such as uncompressed frames or encoded H264 stream (from NVENC).
u8* ffmpeg_send_bufs[MAX_BUFFERED_SEND_FRAMES];

HANDLE ffmpeg_thread;

// Queues and semaphores for communicating between the threads.

SvrAsyncStream<u8*> ffmpeg_write_queue;
SvrAsyncStream<u8*> ffmpeg_read_queue;

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
// Software encoding.

struct PxConvText
{
    const char* format;
    const char* color_space;
};

enum PxConv
{
    PXCONV_YUV420_601 = 0,
    PXCONV_YUV444_601 = 1,
    PXCONV_NV12_601 = 2,
    PXCONV_NV21_601 = 3,

    PXCONV_YUV420_709 = 4,
    PXCONV_YUV444_709 = 5,
    PXCONV_NV12_709 = 6,
    PXCONV_NV21_709 = 7,

    PXCONV_BGR0 = 8,

    NUM_PXCONVS = 9,
};

// Names for ini.
const char* PXFORMAT_TABLE[] = {
    "yuv420",
    "yuv444",
    "nv12",
    "nv21",
    "bgr0",
};

// Names for ini.
const char* COLORSPACE_TABLE[] = {
    "601",
    "709",
    "rgb",
};

// Names for ini and ffmpeg.
const char* ENCODER_TABLE[] = {
    "libx264",
    "libx264rgb",
};

// Names for ini and ffmpeg.
const char* ENCODER_PRESET_TABLE[] = {
    "ultrafast",
    "superfast",
    "veryfast",
    "faster",
    "fast",
    "medium",
    "slow",
    "slower",
    "veryslow",
    "placebo",
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

    // Mapping will take between 400 and 1500 us on average, not much to do about that.
    // We cannot use D3D11_MAP_FLAG_DO_NOT_WAIT here (and advance the cpu texture queue) because of the CopyResource use above which
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
            svr_log("Could not create SWENC texture %d for PXCONV %s+%s (%#x)\n", i, text.format, text.color_space);
            goto rfail;
        }

        hr = d3d11_device->CreateUnorderedAccessView(pxconv_texs[i], NULL, &pxconv_uavs[i]);

        if (FAILED(hr))
        {
            svr_log("Could not create SWENC UAV %d for PXCONV %s+%s (%#x)\n", i, text.format, text.color_space);
            goto rfail;
        }

        tex_desc.Usage = D3D11_USAGE_STAGING;
        tex_desc.BindFlags = 0;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            hr = d3d11_device->CreateTexture2D(&tex_desc, NULL, &pxconv_dls[i].texs[j]);

            if (FAILED(hr))
            {
                svr_log("Could not create SWENC CPU texture %d (rotation %d) for PXCONV %s+%s (%#x)\n", i, j, text.format, text.color_space);
                goto rfail;
            }
        }
    }

rfail:
    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        svr_maybe_release(&pxconv_texs[i]);
        svr_maybe_release(&pxconv_uavs[i]);

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            svr_maybe_release(&pxconv_dls[i].texs[j]);
        }
    }

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

        pxconv_plane_sizes[i] = tex_desc.Width * tex_desc.Height * calc_bytes_pitch(tex_desc.Format);
        pxconv_widths[i] = tex_desc.Width;
        pxconv_heights[i] = tex_desc.Height;
        pxconv_pitches[i] = tex_desc.Width * calc_bytes_pitch(tex_desc.Format);
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
void* nvenc_encoder;
HANDLE nvenc_thread;

SvrSemaphore nvenc_raw_sem;
SvrSemaphore nvenc_enc_sem;

struct NvencPicture
{
    ID3D11Texture2D* tex;
    NV_ENC_REGISTERED_PTR resource;
};

s32 nvenc_num_pics;

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
        svr_log("Could not create D3D11 device for NVENC encoding (%#x)\n", hr);
        goto rfail;
    }

    nvenc_funs.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NvEncodeAPICreateInstance(&nvenc_funs);

    nvenc_open_session_params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    nvenc_open_session_params.device = nvenc_d3d11_device;
    nvenc_open_session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    nvenc_open_session_params.apiVersion = NVENCAPI_VERSION;
    ns = nvenc_funs.nvEncOpenEncodeSessionEx(&nvenc_open_session_params, &nvenc_encoder);

    if (ns != NV_ENC_SUCCESS)
    {
        svr_log("Could not open NVENC encoder (%d)\n", (s32)ns);
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
    svr_maybe_release(&nvenc_d3d11_device);
    svr_maybe_release(&nvenc_d3d11_context);

    if (nvenc_encoder)
    {
        nvenc_funs.nvEncDestroyEncoder(nvenc_encoder);
        nvenc_encoder = NULL;
    }

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

    GUID preset_guid = NV_ENC_PRESET_LOSSLESS_HP_GUID;

    NV_ENC_INITIALIZE_PARAMS nvenc_init_params = {};
    NV_ENC_PRESET_CONFIG preset_config;

    HRESULT hr;

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

    if (ns != NV_ENC_SUCCESS)
    {
        svr_log("Could not initialize the NVENC encoder (%d)\n", (s32)ns);
        goto rfail;
    }

    preset_config.version = NV_ENC_PRESET_CONFIG_VER;
    preset_config.presetCfg.version = NV_ENC_CONFIG_VER;
    nvenc_funs.nvEncGetEncodePresetConfig(nvenc_encoder, NV_ENC_CODEC_H264_GUID, preset_guid, &preset_config);

    // Note: The client should allocate at least (1 + NB) input and output buffers, where NB is the
    // number of B frames between successive P frames.
    nvenc_num_pics = svr_max(4, preset_config.presetCfg.frameIntervalP * 2 * 2);

    for (s32 i = 0; i < nvenc_num_pics; i++)
    {
        D3D11_TEXTURE2D_DESC enc_tex_desc = {};
        enc_tex_desc.Width = movie_width;
        enc_tex_desc.Height = movie_height;
        enc_tex_desc.MipLevels = 1;
        enc_tex_desc.ArraySize = 1;
        enc_tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        enc_tex_desc.SampleDesc.Count = 1;
        enc_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        enc_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        ID3D11Texture2D* nvenc_tex;
        hr = nvenc_d3d11_device->CreateTexture2D(&enc_tex_desc, NULL, &nvenc_tex);

        if (FAILED(hr))
        {
            svr_log("Could not create NVENC textures (%#x)\n", hr);
            goto rfail;
        }

        nvenc_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

        NV_ENC_REGISTER_RESOURCE nvenc_resource = {};
        nvenc_resource.version = NV_ENC_REGISTER_RESOURCE_VER;
        nvenc_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        nvenc_resource.width = movie_width;
        nvenc_resource.height = movie_height;
        nvenc_resource.resourceToRegister = nvenc_tex;
        nvenc_resource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
        nvenc_resource.bufferUsage = NV_ENC_INPUT_IMAGE;

        ns = nvenc_funs.nvEncRegisterResource(nvenc_encoder, &nvenc_resource);

        if (ns != NV_ENC_SUCCESS)
        {
            svr_log("Could not register NVENC textures as NVENC resources (%d)\n", (s32)ns);
            goto rfail;
        }

        // Output of above function is placed here.
        nvenc_resource.registeredResource;
    }

    ret = true;
    goto rexit;

rfail:
    // This stuff can't be destroyed normally. Need to work on that.
    assert(false);

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

        u8* mem;
        bool res1 = ffmpeg_write_queue.pull(&mem);
        assert(res1);

        // This will not return until the data has been read by the remote process.
        // Writing with pipes is very inconsistent and can wary with several milliseconds.
        // It has been tested to use overlapped I/O with completion routines but that was also too inconsistent.
        // This will take about 300 - 6000 us, and if it starts off slow, then it will forever be slow until the computer restarts.

        svr_start_prof(&write_prof);
        WriteFile(ffmpeg_write_pipe, mem, pxconv_total_plane_sizes, NULL, NULL);
        svr_end_prof(&write_prof);

        ffmpeg_read_queue.push(&mem);

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
        svr_log("Could not create shader %s (%#x)", name, hr);
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
        if (!create_a_cs_shader("cf3aa43b232f4624ef5e002a716b67045f45b044", file_mem, SHADER_MEM_SIZE, d3d11_device, &mosample_legacy_cs)) goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        svr_maybe_release(&pxconv_cs[i]);
    }

    svr_maybe_release(&mosample_cs);
    svr_maybe_release(&mosample_legacy_cs);

rexit:
    free(file_mem);

    return ret;
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
    d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &fmt_support2, sizeof(fmt_support2));

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
    svr_log("Using graphics device %x\n", dxgi_adapter_desc.DeviceId);

    svr_log("Typed UAV support: %d\n", (s32)hw_has_typed_uav_support);
    svr_log("NVENC support: %d\n", (s32)hw_has_nvenc_support);

    // Minimum size of a constant buffer is 16 bytes.

    // For mosample_buffer_0.
    D3D11_BUFFER_DESC buf0_desc = {};
    buf0_desc.ByteWidth = 16;
    buf0_desc.Usage = D3D11_USAGE_DYNAMIC;
    buf0_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buf0_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buf0_desc.MiscFlags = 0;

    HRESULT hr;
    hr = d3d11_device->CreateBuffer(&buf0_desc, NULL, &mosample_cb);

    if (FAILED(hr))
    {
        svr_log("Could not create mosample constant buffer (%#x)\n", hr);
        goto rfail;
    }

    if (!hw_has_typed_uav_support)
    {
        // For mosample_buffer_1.
        D3D11_BUFFER_DESC buf1_desc = {};
        buf1_desc.ByteWidth = 16;
        buf1_desc.Usage = D3D11_USAGE_DYNAMIC;
        buf1_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buf1_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buf1_desc.MiscFlags = 0;

        hr = d3d11_device->CreateBuffer(&buf1_desc, NULL, &mosample_legacy_cb);

        if (FAILED(hr))
        {
            svr_log("Could not create legacy mosample constant buffer (%#x)\n", hr);
            goto rfail;
        }
    }

    if (!create_shaders(d3d11_device))
    {
        goto rfail;
    }

    svr_sem_init(&ffmpeg_write_sem, 0, MAX_BUFFERED_SEND_FRAMES);
    svr_sem_init(&ffmpeg_read_sem, MAX_BUFFERED_SEND_FRAMES, MAX_BUFFERED_SEND_FRAMES);

    ffmpeg_write_queue.init(MAX_BUFFERED_SEND_FRAMES);
    ffmpeg_read_queue.init(MAX_BUFFERED_SEND_FRAMES);

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
    svr_maybe_release(&mosample_cb);
    svr_maybe_release(&mosample_legacy_cb);

rexit:
    return ret;
}

s32 atoi_in_range(SvrIniLine* line, s32 min, s32 max)
{
    s32 v = strtol(line->value, NULL, 10);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %d, max is %d, value is %d) setting to %d\n", line->title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

float atof_in_range(SvrIniLine* line, float min, float max)
{
    float v = atof(line->value);

    if (v < min || v > max)
    {
        float new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %0.2f, max is %0.2f, value is %0.2f) setting to %0.2f\n", line->title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

const char* str_in_list_or(SvrIniLine* line, const char** list, s32 num, const char* def)
{
    for (s32 i = 0; i < num; i++)
    {
        if (!strcmp(list[i], line->value))
        {
            return list[i];
        }
    }

    const s32 OPTS_SIZE = 1024;

    char opts[OPTS_SIZE];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        StringCchCatA(opts, OPTS_SIZE, list[i]);

        if (i != num - 1)
        {
            StringCchCatA(opts, OPTS_SIZE, ", ");
        }
    }

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line->title, line->value, opts, def);

    return def;
}

const char* rl_map_str_in_list(s32 value, StrIntMapping* mappings, s32 num)
{
    for (s32 i = 0; i < num; i++)
    {
        StrIntMapping& m = mappings[i];

        if (m.value == value)
        {
            return m.name;
        }
    }

    return NULL;
}

s32 map_str_in_list_or(SvrIniLine* line, StrIntMapping* mappings, s32 num, s32 def)
{
    for (s32 i = 0; i < num; i++)
    {
        StrIntMapping& m = mappings[i];

        if (!strcmp(m.name, line->value))
        {
            return m.value;
        }
    }

    const s32 OPTS_SIZE = 1024;

    char opts[OPTS_SIZE];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        StringCchCatA(opts, OPTS_SIZE, mappings[i].name);

        if (i != num - 1)
        {
            StringCchCatA(opts, OPTS_SIZE, ", ");
        }
    }

    const char* def_title = rl_map_str_in_list(def, mappings, num);

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line->title, line->value, opts, def_title);

    return def;
}

bool read_profile(const char* profile)
{
    char full_profile_path[MAX_PATH];
    full_profile_path[0] = 0;
    StringCchCatA(full_profile_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_profile_path, MAX_PATH, "\\data\\profiles\\");
    StringCchCatA(full_profile_path, MAX_PATH, profile);
    StringCchCatA(full_profile_path, MAX_PATH, ".ini");

    SvrIniMem ini_mem;

    if (!svr_open_ini_read(full_profile_path, &ini_mem))
    {
        return false;
    }

    SvrIniLine ini_line = svr_alloc_ini_line();
    SvrIniTokenType ini_token_type;

    #define OPT_S32(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atoi_in_range(&ini_line, MIN, MAX); }
    #define OPT_FLOAT(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atof_in_range(&ini_line, MIN, MAX); }
    #define OPT_STR(NAME, VAR, SIZE) (!strcmp(ini_line.title, NAME)) { StringCchCopyA(VAR, SIZE, ini_line.value); }
    #define OPT_STR_LIST(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF); }
    #define OPT_STR_MAP(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = decltype(VAR)(map_str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF)); }

    MovieProfile& p = movie_profile;

    while (svr_read_ini(&ini_mem, &ini_line, &ini_token_type))
    {
        if OPT_S32("video_fps", p.movie_fps, 1, 1000)
        else if OPT_STR_LIST("video_encoder", p.sw_encoder, ENCODER_TABLE, "libx264")
        else if OPT_STR_LIST("video_pixel_format", p.sw_pxformat, PXFORMAT_TABLE, "yuv420")
        else if OPT_STR_LIST("video_colorspace", p.sw_colorspace, COLORSPACE_TABLE, "601")
        else if OPT_S32("video_x264_crf", p.sw_crf, 0, 52)
        else if OPT_STR_LIST("video_x264_preset", p.sw_x264_preset, ENCODER_PRESET_TABLE, "veryfast")
        else if OPT_S32("video_x264_intra", p.sw_x264_intra, 0, 1)
        else if OPT_S32("motion_blur_enabled", p.mosample_enabled, 0, 1)
        else if OPT_S32("motion_blur_fps_mult", p.mosample_mult, 1, INT32_MAX)
        else if OPT_FLOAT("motion_blur_frame_exposure", p.mosample_exposure, 0.0f, 1.0f)
        else if OPT_S32("velocity_overlay_enabled", p.veloc_enabled, 0, 1)
        else if OPT_STR("velocity_overlay_font_family", p.veloc_font, MAX_VELOC_FONT_NAME)
        else if OPT_S32("velocity_overlay_font_size", p.veloc_font_size, 0, INT32_MAX)
        else if OPT_S32("velocity_overlay_color_r", p.veloc_font_color[0], 0, 255)
        else if OPT_S32("velocity_overlay_color_g", p.veloc_font_color[1], 0, 255)
        else if OPT_S32("velocity_overlay_color_b", p.veloc_font_color[2], 0, 255)
        else if OPT_S32("velocity_overlay_color_a", p.veloc_font_color[3], 0, 255)
        else if OPT_STR_MAP("velocity_overlay_font_style", p.veloc_font_style, FONT_STYLE_TABLE, DWRITE_FONT_STYLE_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_font_weight", p.veloc_font_weight, FONT_WEIGHT_TABLE, DWRITE_FONT_WEIGHT_BOLD)
        else if OPT_STR_MAP("velocity_overlay_font_stretch", p.veloc_font_stretch, FONT_STRETCH_TABLE, DWRITE_FONT_STRETCH_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_text_align", p.veloc_text_align, TEXT_ALIGN_TABLE, DWRITE_TEXT_ALIGNMENT_CENTER)
        else if OPT_STR_MAP("velocity_overlay_paragraph_align", p.veloc_para_align, PARAGRAPH_ALIGN_TABLE, DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
        else if OPT_S32("velocity_overlay_padding", p.veloc_padding, 0, INT32_MAX)
    }

    svr_free_ini_line(&ini_line);
    svr_close_ini(&ini_mem);

    #undef OPT_S32
    #undef OPT_FLOAT
    #undef OPT_STR
    #undef OPT_STR_LIST
    #undef OPT_STR_MAP

    return true;
}

bool verify_profile()
{
    // We discard the movie if these are not right, because we don't want to spend
    // a very long time creating a movie which then would get thrown away since it didn't use the correct settings.

    if (!strcmp(movie_profile.sw_encoder, "libx264"))
    {
        if (!strcmp(movie_profile.sw_pxformat, "bgr0"))
        {
            game_log("The libx264 encoder can only use YUV pixel formats\n");
            return false;
        }

        if (!strcmp(movie_profile.sw_colorspace, "rgb"))
        {
            game_log("The libx264 encoder can only use YUV color spaces\n");
            return false;
        }
    }

    else if (!strcmp(movie_profile.sw_encoder, "libx264rgb"))
    {
        if (!strcmp(movie_profile.sw_pxformat, "bgr0") != 0)
        {
            game_log("The libx264rgb encoder can only use the rgb pixel format\n");
            return false;
        }

        if (!strcmp(movie_profile.sw_colorspace, "rgb") != 0)
        {
            game_log("The libx264rgb encoder can only use the rgb color space\n");
            return false;
        }
    }

    if (movie_profile.mosample_mult == 1)
    {
        game_log("motion_blur_fps_mult is set to 1, which doesn't enable motion blur\n");
        return false;
    }

    return true;
}

void set_default_profile()
{
    MovieProfile& p = movie_profile;

    p.movie_fps = 60;
    p.sw_encoder = "libx264";
    p.sw_pxformat = "yuv420";
    p.sw_colorspace = "601";
    p.sw_crf = 23;
    p.sw_x264_preset = "veryfast";
    p.sw_x264_intra = 0;
    p.mosample_enabled = 1;
    p.mosample_mult = 60;
    p.mosample_exposure = 0.5f;
    p.veloc_enabled = 0;
    StringCchCopyA(p.veloc_font, MAX_VELOC_FONT_NAME, "Arial");
    p.veloc_font_size = 72;
    p.veloc_font_color[0] = 255;
    p.veloc_font_color[1] = 255;
    p.veloc_font_color[2] = 255;
    p.veloc_font_color[3] = 255;
    p.veloc_font_style = DWRITE_FONT_STYLE_NORMAL;
    p.veloc_font_weight = DWRITE_FONT_WEIGHT_BOLD;
    p.veloc_font_stretch = DWRITE_FONT_STRETCH_NORMAL;
    p.veloc_text_align = DWRITE_TEXT_ALIGNMENT_CENTER;
    p.veloc_para_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    p.veloc_padding = 100;
}

void log_profile()
{
    MovieProfile& p = movie_profile;

    game_log("Using profile:\n");
    game_log("Movie fps: %d\n", p.movie_fps);
    game_log("Video encoder: %s\n", p.sw_encoder);
    game_log("Video pixel format: %s\n", p.sw_pxformat);
    game_log("Video colorspace: %s\n", p.sw_colorspace);
    game_log("Video crf: %d\n", p.sw_crf);
    game_log("Video x264 preset: %s\n", p.sw_x264_preset);
    game_log("Video x264 intra: %d\n", p.sw_x264_intra);
    game_log("Use motion blur: %d\n", p.mosample_enabled);

    if (p.mosample_enabled)
    {
        game_log("Motion blur multiplier: %d\n", p.mosample_mult);
        game_log("Motion blur exposure: %0.2f\n", p.mosample_exposure);
    }

    game_log("Use velocity overlay: %d\n", p.veloc_enabled);

    if (p.veloc_enabled)
    {
        game_log("Velocity font: %s\n", p.veloc_font);
        game_log("Velocity font size: %d\n", p.veloc_font_size);
        game_log("Velocity font color: %d %d %d %d\n", p.veloc_font_color[0], p.veloc_font_color[1], p.veloc_font_color[2], p.veloc_font_color[3]);
        game_log("Velocity font style: %s\n", rl_map_str_in_list(p.veloc_font_style, FONT_STYLE_TABLE, SVR_ARRAY_SIZE(FONT_STYLE_TABLE)));
        game_log("Velocity font weight: %s\n", rl_map_str_in_list(p.veloc_font_weight, FONT_WEIGHT_TABLE, SVR_ARRAY_SIZE(FONT_WEIGHT_TABLE)));
        game_log("Velocity font stretch: %s\n", rl_map_str_in_list(p.veloc_font_stretch, FONT_STRETCH_TABLE, SVR_ARRAY_SIZE(FONT_STRETCH_TABLE)));
        game_log("Velocity text align: %s\n", rl_map_str_in_list(p.veloc_text_align, TEXT_ALIGN_TABLE, SVR_ARRAY_SIZE(TEXT_ALIGN_TABLE)));
        game_log("Velocity paragraph align: %s\n", rl_map_str_in_list(p.veloc_para_align, PARAGRAPH_ALIGN_TABLE, SVR_ARRAY_SIZE(PARAGRAPH_ALIGN_TABLE)));
        game_log("Velocity text padding: %d\n", p.veloc_padding);
    }
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

// We start a separate process for encoding for two reasons:
// 1) Source is a 32-bit engine, and it was common to run out of memory in games such as CSGO that uses a lot of memory.
// 2) The ffmpeg API is horrible to work with with an incredible amount of pitfalls that will grant you a media that is slighly incorrect
//    and there is no reliable documentation.
// Data is sent to this process through a pipe that we create.
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
        svr_log("Could not create ffmpeg process pipes (%lu)\n", GetLastError());
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

    if (!CreateProcessA(full_ffmpeg_path, full_args, NULL, NULL, TRUE, create_flags, NULL, svr_resource_path, &start_info, &proc_info))
    {
        game_log("Could not create ffmpeg process (%lu)\n", GetLastError());
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
    assert(ffmpeg_read_queue.read_buffer_health() == MAX_BUFFERED_SEND_FRAMES);

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

    // If this doesn't work then we use the default profile in code.
    if (!read_profile(profile))
    {
        game_log("Could not load profile %s, setting default\n", profile);
        set_default_profile();
    }

    else
    {
        if (!verify_profile())
        {
            game_log("Could not verify profile, setting default\n");
            set_default_profile();
        }
    }

    assert(verify_profile());

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
        if (FAILED(hr)) goto rfail;

        hr = d3d11_device->CreateShaderResourceView(work_tex, NULL, &work_tex_srv);
        if (FAILED(hr)) goto rfail;

        hr = d3d11_device->CreateRenderTargetView(work_tex, NULL, &work_tex_rtv);
        if (FAILED(hr)) goto rfail;

        hr = d3d11_device->CreateUnorderedAccessView(work_tex, NULL, &work_tex_uav);
        if (FAILED(hr)) goto rfail;
    }

    else
    {
        D3D11_BUFFER_DESC work_buf_desc = {};
        work_buf_desc.ByteWidth = sizeof(float) * 4 * tex_desc.Width * tex_desc.Height;
        work_buf_desc.Usage = D3D11_USAGE_DEFAULT;
        work_buf_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        work_buf_desc.CPUAccessFlags = 0;
        work_buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        work_buf_desc.StructureByteStride = sizeof(float) * 4;

        hr = d3d11_device->CreateBuffer(&work_buf_desc, NULL, &work_buf_legacy_sb);
        if (FAILED(hr)) goto rfail;

        hr = d3d11_device->CreateShaderResourceView(work_buf_legacy_sb, NULL, &work_buf_legacy_sb_srv);
        if (FAILED(hr)) goto rfail;

        hr = d3d11_device->CreateUnorderedAccessView(work_buf_legacy_sb, NULL, &work_buf_legacy_sb_uav);
        if (FAILED(hr)) goto rfail;

        // For dest_texture_width.
        update_constant_buffer(d3d11_context, mosample_legacy_cb, &tex_desc.Width, sizeof(UINT));
    }

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
    {
        ffmpeg_send_bufs[i] = (u8*)malloc(pxconv_total_plane_sizes);
    }

    // Need to overwrite with new data.
    ffmpeg_read_queue.reset();

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
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

    game_log("Starting movie to %s (%dx%d)\n", dest, movie_width, movie_height);

    log_profile();

    ret = true;
    goto rexit;

rfail:
    svr_maybe_release(&work_tex);
    svr_maybe_release(&work_tex_srv);
    svr_maybe_release(&work_tex_rtv);
    svr_maybe_release(&work_tex_uav);
    svr_maybe_release(&work_buf_legacy_sb);
    svr_maybe_release(&work_buf_legacy_sb_srv);
    svr_maybe_release(&work_buf_legacy_sb_uav);

rexit:
    return ret;
}

void send_converted_video_frame_to_ffmpeg(ID3D11DeviceContext* d3d11_context)
{
    svr_sem_wait(&ffmpeg_read_sem);

    u8* mem;
    bool res1 = ffmpeg_read_queue.pull(&mem);
    assert(res1);

    svr_start_prof(&dl_prof);
    download_textures(d3d11_context, pxconv_texs, pxconv_dls, used_pxconv_planes, mem, pxconv_total_plane_sizes);
    svr_end_prof(&dl_prof);

    ffmpeg_write_queue.push(&mem);

    svr_sem_release(&ffmpeg_write_sem);
}

void motion_sample(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv, float weight)
{
    if (weight != mosample_weight_cache)
    {
        mosample_weight_cache = weight;
        update_constant_buffer(d3d11_context, mosample_cb, &weight, sizeof(float));
    }

    if (hw_has_typed_uav_support)
    {
        d3d11_context->CSSetShader(mosample_cs, NULL, 0);
        d3d11_context->CSSetConstantBuffers(0, 1, &mosample_cb);
        d3d11_context->CSSetUnorderedAccessViews(0, 1, &work_tex_uav, NULL);
        d3d11_context->CSSetShaderResources(0, 1, &game_content_srv);
    }

    else
    {
        ID3D11Buffer* legacy_cbs[] = { mosample_cb, mosample_legacy_cb };
        d3d11_context->CSSetShader(mosample_legacy_cs, NULL, 0);
        d3d11_context->CSSetConstantBuffers(0, 2, legacy_cbs);
        d3d11_context->CSSetUnorderedAccessViews(0, 1, &work_buf_legacy_sb_uav, NULL);
        d3d11_context->CSSetShaderResources(0, 1, &game_content_srv);
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
    convert_pixel_formats(d3d11_context, srv);
    send_converted_video_frame_to_ffmpeg(d3d11_context);
}

void proc_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv)
{
    if (frame_num > 0)
    {
        svr_start_prof(&frame_prof);

        if (movie_profile.mosample_enabled)
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

                float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                d3d11_context->ClearRenderTargetView(work_tex_rtv, clear_color);

                if (mosample_remainder > FLT_EPSILON && mosample_remainder > (1.0f - exposure))
                {
                    weight = ((mosample_remainder - (1.0f - exposure)) * (1.0f / exposure));
                    motion_sample(d3d11_context, game_content_srv, weight);
                }
            }
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

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
    {
        svr_maybe_free(&ffmpeg_send_bufs[i]);
    }

    if (hw_has_typed_uav_support)
    {
        svr_maybe_release(&work_tex);
        svr_maybe_release(&work_tex_rtv);
        svr_maybe_release(&work_tex_srv);
        svr_maybe_release(&work_tex_uav);
    }

    else
    {
        svr_maybe_release(&work_buf_legacy_sb);
        svr_maybe_release(&work_buf_legacy_sb_srv);
        svr_maybe_release(&work_buf_legacy_sb_uav);
    }

    for (s32 i = 0; i < used_pxconv_planes; i++)
    {
        svr_maybe_release(&pxconv_texs[i]);

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            svr_maybe_release(&pxconv_dls[i].texs[j]);
        }

        svr_maybe_release(&pxconv_uavs[i]);
    }

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
