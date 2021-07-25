#include "game_shared.h"
#include "game_proc.h"
#include <dxgi.h>
#include <d3d11.h>
#include "svr_ini.h"
#include <strsafe.h>
#include <dwrite.h>
#include <d2d1.h>
#include <malloc.h>
#include <assert.h>
#include <limits>
#include "svr_prof.h"
#include "svr_atom.h"
#include "svr_mem_range.h"
#include "svr_stream.h"
#include "svr_sem.h"

struct PxConvText
{
    const char* format;
    const char* color_space;
};

enum PxConv
{
    PXCONV_YUV420_601,
    PXCONV_YUV444_601,
    PXCONV_NV12_601,
    PXCONV_NV21_601,

    PXCONV_YUV420_709,
    PXCONV_YUV444_709,
    PXCONV_NV12_709,
    PXCONV_NV21_709,

    PXCONV_BGR0,

    NUM_PXCONVS
};

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

struct StrIntMapping
{
    const char* name;
    s32 value;
};

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

ID3D11Device* proc_d3d11_device;
ID3D11DeviceContext* proc_d3d11_context;

// These texture are created when the movie starts, and destroyed when the movie ends.
// This contains the game texture, not updated here. Read only!
ID3D11Texture2D* proc_game_content_tex;
ID3D11ShaderResourceView* proc_game_content_srv;

float proc_mosample_remainder;
float proc_mosample_remainder_step;

ID3D11PixelShader* proc_texture_ps;
ID3D11VertexShader* proc_overlay_vs;

ID3D11Texture2D* proc_work_tex;
ID3D11RenderTargetView* proc_work_tex_rtv;
ID3D11ShaderResourceView* proc_work_tex_srv;
ID3D11UnorderedAccessView* proc_work_tex_uav;

// For when we don't have typed UAV loads and stores we have to use buffers
// instead of textures. So this buffer contains the equivalent 32 bit floats for R32G32B32A32.
// Because it is not a texture, it needs an additional constant buffer that tells the width so we can calculate the index.
ID3D11Buffer* proc_work_buf_legacy_sb;
ID3D11ShaderResourceView* proc_work_buf_legacy_sb_srv;
ID3D11UnorderedAccessView* proc_work_buf_legacy_sb_uav;

ID3D11ComputeShader* proc_mosample_cs;
ID3D11ComputeShader* proc_mosample_legacy_cs;

// Both these constant buffers should be merged into one instead and the shader preprocessor selects what data to have.
ID3D11Buffer* proc_mosample_cb;
ID3D11Buffer* proc_mosample_legacy_cb;

// To not upload data all the time.
float proc_mosample_weight_cache;

ID3D11ComputeShader* proc_pxconv_cs[NUM_PXCONVS];

const s32 NUM_BUFFERED_DL_TEXS = 2;

// Rotation of CPU textures to download to.
struct PxConvTexDl
{
    // Keep 3 textures rotating for downloads.
    ID3D11Texture2D* texs[NUM_BUFFERED_DL_TEXS];
    s32 i;

    ID3D11Texture2D* get_current()
    {
        return texs[i];
    }

    void advance()
    {
        i++;
        i &= (NUM_BUFFERED_DL_TEXS - 1);
    }
};

// Up to 3 planes used for YUV video.
ID3D11Texture2D* proc_pxconv_texs[3];
ID3D11UnorderedAccessView* proc_pxconv_uavs[3];

PxConvTexDl proc_pxconv_dls[3];

UINT proc_pxconv_pitches[3];
UINT proc_pxconv_widths[3];
UINT proc_pxconv_heights[3];

s32 proc_used_pxconv_planes;
s32 proc_pxconv_plane_sizes[3];
s32 proc_pxconv_total_plane_sizes;

s32 proc_frame_num;
s32 proc_movie_width;
s32 proc_movie_height;

SvrProf frame_prof;
SvrProf dl_prof;
SvrProf write_prof;
SvrProf mosample_prof;

s32 proc_movie_encode_threads;
s32 proc_movie_fps;
const char* proc_movie_encoder;
const char* proc_movie_pxformat;
const char* proc_movie_colorspace;
s32 proc_movie_crf;
const char* proc_movie_x264_preset;
s32 proc_movie_x264_intra;

s32 proc_mosample_enabled;
s32 proc_mosample_mult;
float proc_mosample_exposure;

const s32 MAX_VELOC_FONT_NAME = 128;

s32 proc_veloc_enabled;
char proc_veloc_font[MAX_VELOC_FONT_NAME];
s32 proc_veloc_font_size;
s32 proc_veloc_font_color[4];
DWRITE_FONT_STYLE proc_veloc_font_style;
DWRITE_FONT_WEIGHT proc_veloc_font_weight;
DWRITE_FONT_STRETCH proc_veloc_font_stretch;
DWRITE_TEXT_ALIGNMENT proc_veloc_text_align;
DWRITE_PARAGRAPH_ALIGNMENT proc_veloc_para_align;
s32 proc_veloc_padding;

PxConv proc_movie_pxconv;
char proc_movie_path[MAX_PATH];

// HW caps:

bool hw_has_typed_uav_support;
bool hw_has_nvenc_support;

// We write data to the ffmpeg process through this pipe.
// It is redirected to their stdin.
HANDLE proc_ffmpeg_write_pipe;
HANDLE proc_ffmpeg_proc;

// How many completed (uncompressed) frames we keep in memory waiting to be sent to ffmpeg.
const s32 MAX_BUFFERED_SEND_FRAMES = 8;

// The buffer that is sent to the ffmpeg process.
// Each entry is large enough to contain every pxconv plane.
u8* proc_ffmpeg_send_bufs[MAX_BUFFERED_SEND_FRAMES];

SvrAsyncStream<u8*> proc_ffmpeg_write_queue;
SvrAsyncStream<u8*> proc_ffmpeg_read_queue;

HANDLE proc_ffmpeg_thread;

// Semaphore that is signalled when there are frames to send to ffmpeg (pulls from proc_ffmpeg_write_queue).
// This is incremented by the game thread when it has added a downloaded frame to the write queue.
SvrSemaphore proc_ffmpeg_write_sem;

// Semaphore that is signalled when there are frames available to download into (pulls from proc_ffmpeg_read_queue).
// This is incremented by the ffmpeg thread when it has sent a frame to the ffmpeg process.
SvrSemaphore proc_ffmpeg_read_sem;

void update_constant_buffer(ID3D11Buffer* buffer, const void* data, UINT size)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    proc_d3d11_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    memcpy(mapped.pData, data, size);

    proc_d3d11_context->Unmap(buffer, 0);
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

// Calculates the thread group count to use for specific dimensions.
s32 calc_cs_thread_groups(s32 input)
{
    // Thread group divisor constant must match the thread count in the compute shaders!

    // This number is arbitrary, and may end up with better or worse performance depending on the hardware.
    return ((float)input / 8.0f) + 0.5f;
}

void convert_pixel_formats(ID3D11ShaderResourceView* source_srv)
{
    proc_d3d11_context->CSSetShader(proc_pxconv_cs[proc_movie_pxconv], NULL, 0);
    proc_d3d11_context->CSSetShaderResources(0, 1, &source_srv);
    proc_d3d11_context->CSSetUnorderedAccessViews(0, proc_used_pxconv_planes, proc_pxconv_uavs, NULL);

    proc_d3d11_context->Dispatch(calc_cs_thread_groups(proc_movie_width), calc_cs_thread_groups(proc_movie_height), 1);

    ID3D11ShaderResourceView* null_srvs[] = { NULL };
    ID3D11UnorderedAccessView* null_uavs[] = { NULL };

    proc_d3d11_context->CSSetShaderResources(0, 1, null_srvs);
    proc_d3d11_context->CSSetUnorderedAccessViews(0, 1, null_uavs, NULL);
}

// This thread will write data to the ffmpeg process.
// Writing to the pipe is real slow and we want to buffer up a few to send which it can work on.
DWORD WINAPI ffmpeg_thread_proc(LPVOID lpParameter)
{
    while (true)
    {
        svr_sem_wait(&proc_ffmpeg_write_sem);

        u8* mem;
        bool res1 = proc_ffmpeg_write_queue.pull(&mem);
        assert(res1);

        // This will not return until the data has been read by the remote process.
        // Writing with pipes is very inconsistent and can wary with several milliseconds.
        // It has been tested to use overlapped I/O with completion routines but that was also too inconsistent.
        // This will take about 300 - 6000 us, and if it starts off slow, then it will forever be slow until the computer restarts.

        svr_start_prof(write_prof);
        WriteFile(proc_ffmpeg_write_pipe, mem, proc_pxconv_total_plane_sizes, NULL, NULL);
        svr_end_prof(write_prof);

        proc_ffmpeg_read_queue.push(mem);

        svr_sem_release(&proc_ffmpeg_read_sem);
    }
}

void download_textures(ID3D11Texture2D** gpu_texes, PxConvTexDl* cpu_texes, s32 num_texes, void* dest, s32 size)
{
    // Need to copy the textures into readable memory.

    for (s32 i = 0; i < num_texes; i++)
    {
        proc_d3d11_context->CopyResource(cpu_texes[i].get_current(), gpu_texes[i]);
    }

    D3D11_MAPPED_SUBRESOURCE* maps = (D3D11_MAPPED_SUBRESOURCE*)_alloca(sizeof(D3D11_MAPPED_SUBRESOURCE) * num_texes);
    void** map_datas = (void**)_alloca(sizeof(void*) * num_texes);
    UINT* row_pitches = (UINT*)_alloca(sizeof(UINT) * num_texes);

    // Mapping will take between 400 and 1500 us on average, not much to do about that.
    // We cannot use D3D11_MAP_FLAG_DO_NOT_WAIT here (and advance the cpu texture queue) because of the CopyResource use above which
    // may not have finished yet.

    for (s32 i = 0; i < num_texes; i++)
    {
        proc_d3d11_context->Map(cpu_texes[i].get_current(), 0, D3D11_MAP_READ, 0, &maps[i]);
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

    s32 offset = 0;

    for (s32 i = 0; i < num_texes; i++)
    {
        u8* source_ptr = (u8*)map_datas[i];
        u8* dest_ptr = (u8*)dest + offset;

        for (UINT j = 0; j < proc_pxconv_heights[i]; j++)
        {
            memcpy(dest_ptr, source_ptr, proc_pxconv_pitches[i]);

            source_ptr += row_pitches[i];
            dest_ptr += proc_pxconv_pitches[i];
        }

        offset += proc_pxconv_pitches[i] * proc_pxconv_heights[i];
    }

    for (s32 i = 0; i < num_texes; i++)
    {
        proc_d3d11_context->Unmap(cpu_texes[i].get_current(), 0);
    }

    for (s32 i = 0; i < num_texes; i++)
    {
        cpu_texes[i].advance();
    }
}

void load_one_shader(const char* name, void* buf, s32 buf_size, DWORD* shader_size)
{
    char full_shader_path[MAX_PATH];
    full_shader_path[0] = 0;
    StringCchCatA(full_shader_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_shader_path, MAX_PATH, "\\data\\shaders\\");
    StringCchCatA(full_shader_path, MAX_PATH, name);

    HANDLE h = CreateFileA(full_shader_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    ReadFile(h, buf, buf_size, shader_size, NULL);

    CloseHandle(h);
}

void create_shaders()
{
    const s32 SHADER_BUF_SIZE = 8192;

    void* file_mem = malloc(SHADER_BUF_SIZE);

    DWORD shader_size;

    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        load_one_shader(PXCONV_SHADER_NAMES[i], file_mem, SHADER_BUF_SIZE, &shader_size);
        proc_d3d11_device->CreateComputeShader(file_mem, shader_size, NULL, &proc_pxconv_cs[i]);
    }

    load_one_shader("c52620855f15b2c47b8ca24b890850a90fdc7017", file_mem, SHADER_BUF_SIZE, &shader_size);
    proc_d3d11_device->CreateComputeShader(file_mem, shader_size, NULL, &proc_mosample_cs);

    load_one_shader("cf3aa43b232f4624ef5e002a716b67045f45b044", file_mem, SHADER_BUF_SIZE, &shader_size);
    proc_d3d11_device->CreateComputeShader(file_mem, shader_size, NULL, &proc_mosample_legacy_cs);

    free(file_mem);
}

void proc_init()
{
    // BGRA support needed for Direct2D interoperability.
    // It is also only intended to be used from a single thread.
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    #if SVR_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    // Should be good enough for all the features that we make use of.
    const D3D_FEATURE_LEVEL MINIMUM_VERSION = D3D_FEATURE_LEVEL_11_0;

    D3D_FEATURE_LEVEL levels[] = {
        MINIMUM_VERSION
    };

    D3D_FEATURE_LEVEL created_level;

    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, levels, 1, D3D11_SDK_VERSION, &proc_d3d11_device, &created_level, &proc_d3d11_context);

    // See if typed UAV loads and stores are supported so we can decide what code path to use.
    // It becomes more complicated without this, but still doable. More reports than expected have come out regarding the absence
    // of this feature in certain hardware.

    // If we have hardware support for this, we can add the game content directly to the work texture and be on our way.
    // If we don't we have to create a structured buffer that we can motion sample on instead.
    // Both ways will be identical in memory but hardware support differs.

    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fmt_support2;
    fmt_support2.InFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    proc_d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &fmt_support2, sizeof(fmt_support2));

    hw_has_typed_uav_support = (fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) && (fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE);

    svr_log("Typed UAV support: %d\n", (s32)hw_has_typed_uav_support);

    // Minimum size of a constant buffer is 16 bytes.

    // For mosample_buffer_0.
    D3D11_BUFFER_DESC buf0_desc = {};
    buf0_desc.ByteWidth = 16;
    buf0_desc.Usage = D3D11_USAGE_DYNAMIC;
    buf0_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buf0_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buf0_desc.MiscFlags = 0;

    proc_d3d11_device->CreateBuffer(&buf0_desc, NULL, &proc_mosample_cb);

    if (!hw_has_typed_uav_support)
    {
        // For mosample_buffer_1.
        D3D11_BUFFER_DESC buf1_desc = {};
        buf1_desc.ByteWidth = 16;
        buf1_desc.Usage = D3D11_USAGE_DYNAMIC;
        buf1_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buf1_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buf1_desc.MiscFlags = 0;

        proc_d3d11_device->CreateBuffer(&buf1_desc, NULL, &proc_mosample_legacy_cb);
    }

    create_shaders();

    svr_sem_init(&proc_ffmpeg_write_sem, 0, MAX_BUFFERED_SEND_FRAMES);
    svr_sem_init(&proc_ffmpeg_read_sem, MAX_BUFFERED_SEND_FRAMES, MAX_BUFFERED_SEND_FRAMES);

    const s32 QUEUE_RANGE_SIZE = 4 * 1024;

    SvrMemoryRange queue_range;
    queue_range.start = calloc(QUEUE_RANGE_SIZE, 1);
    queue_range.used = 0;
    queue_range.size = QUEUE_RANGE_SIZE;

    proc_ffmpeg_write_queue.init_with_range(queue_range, MAX_BUFFERED_SEND_FRAMES);
    proc_ffmpeg_read_queue.init_with_range(queue_range, MAX_BUFFERED_SEND_FRAMES);

    proc_ffmpeg_thread = CreateThread(NULL, 0, ffmpeg_thread_proc, NULL, 0, NULL);
}

s32 atoi_in_range(SvrIniLine& line, s32 min, s32 max)
{
    s32 v = strtol(line.value, NULL, 10);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(new_v, min, max);

        game_log("Option %s out of range (min is %d, max is %d, value is %d) setting to %d\n", line.title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

float atof_in_range(SvrIniLine& line, float min, float max)
{
    float v = atof(line.value);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(new_v, min, max);

        game_log("Option %s out of range (min is %0.2f, max is %0.2f, value is %0.2f) setting to %0.2f\n", line.title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

const char* str_in_list_or(SvrIniLine& line, const char** list, s32 num, const char* def)
{
    for (s32 i = 0; i < num; i++)
    {
        if (strcmp(list[i], line.value) == 0)
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

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line.title, line.value, opts, def);

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

s32 map_str_in_list_or(SvrIniLine& line, StrIntMapping* mappings, s32 num, s32 def)
{
    for (s32 i = 0; i < num; i++)
    {
        StrIntMapping& m = mappings[i];

        if (strcmp(m.name, line.value) == 0)
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

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line.title, line.value, opts, def_title);

    return def;
}

bool read_profile(const char* profile)
{
    extern const char* svr_resource_path;

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
    /* SvrIniTokenType */ s32 ini_token_type;

    #define OPT_S32(NAME, VAR, MIN, MAX) (strcmp(ini_line.title, NAME) == 0) { VAR = atoi_in_range(ini_line, MIN, MAX); }
    #define OPT_FLOAT(NAME, VAR, MIN, MAX) (strcmp(ini_line.title, NAME) == 0) { VAR = atof_in_range(ini_line, MIN, MAX); }
    #define OPT_STR(NAME, VAR, SIZE) (strcmp(ini_line.title, NAME) == 0) { StringCchCopyA(VAR, SIZE, ini_line.value); }
    #define OPT_STR_LIST(NAME, VAR, LIST, DEF) (strcmp(ini_line.title, NAME) == 0) { VAR = str_in_list_or(ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF); }
    #define OPT_STR_MAP(NAME, VAR, LIST, DEF) (strcmp(ini_line.title, NAME) == 0) { VAR = decltype(VAR)(map_str_in_list_or(ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF)); }

    while (svr_read_ini(ini_mem, &ini_line, &ini_token_type))
    {
        if OPT_S32("encoding_threads", proc_movie_encode_threads, 0, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS))
        else if OPT_S32("video_fps", proc_movie_fps, 1, 1000)
        else if OPT_STR_LIST("video_encoder", proc_movie_encoder, ENCODER_TABLE, "libx264")
        else if OPT_STR_LIST("video_pixel_format", proc_movie_pxformat, PXFORMAT_TABLE, "yuv420")
        else if OPT_STR_LIST("video_colorspace", proc_movie_colorspace, COLORSPACE_TABLE, "601")
        else if OPT_S32("video_x264_crf", proc_movie_crf, 0, 52)
        else if OPT_STR_LIST("video_x264_preset", proc_movie_x264_preset, ENCODER_PRESET_TABLE, "veryfast")
        else if OPT_S32("video_x264_intra", proc_movie_x264_intra, 0, 1)
        else if OPT_S32("motion_blur_enabled", proc_mosample_enabled, 0, 1)
        else if OPT_S32("motion_blur_fps_mult", proc_mosample_mult, 1, INT32_MAX)
        else if OPT_FLOAT("motion_blur_frame_exposure", proc_mosample_exposure, 0.0f, 1.0f)
        else if OPT_S32("velocity_overlay_enabled", proc_veloc_enabled, 0, 1)
        else if OPT_STR("velocity_overlay_font_family", proc_veloc_font, MAX_VELOC_FONT_NAME)
        else if OPT_S32("velocity_overlay_font_size", proc_veloc_font_size, 0, INT32_MAX)
        else if OPT_S32("velocity_overlay_color_r", proc_veloc_font_color[0], 0, 255)
        else if OPT_S32("velocity_overlay_color_g", proc_veloc_font_color[1], 0, 255)
        else if OPT_S32("velocity_overlay_color_b", proc_veloc_font_color[2], 0, 255)
        else if OPT_S32("velocity_overlay_color_a", proc_veloc_font_color[3], 0, 255)
        else if OPT_STR_MAP("velocity_overlay_font_style", proc_veloc_font_style, FONT_STYLE_TABLE, DWRITE_FONT_STYLE_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_font_weight", proc_veloc_font_weight, FONT_WEIGHT_TABLE, DWRITE_FONT_WEIGHT_BOLD)
        else if OPT_STR_MAP("velocity_overlay_font_stretch", proc_veloc_font_stretch, FONT_STRETCH_TABLE, DWRITE_FONT_STRETCH_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_text_align", proc_veloc_text_align, TEXT_ALIGN_TABLE, DWRITE_TEXT_ALIGNMENT_CENTER)
        else if OPT_STR_MAP("velocity_overlay_paragraph_align", proc_veloc_para_align, PARAGRAPH_ALIGN_TABLE, DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
        else if OPT_S32("velocity_overlay_padding", proc_veloc_padding, 0, INT32_MAX)
    }

    svr_free_ini_line(ini_line);
    svr_close_ini(ini_mem);

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

    if (strcmp(proc_movie_encoder, "libx264") == 0)
    {
        if (strcmp(proc_movie_pxformat, "bgr0") == 0)
        {
            game_log("The libx264 encoder can only use YUV pixel formats\n");
            return false;
        }

        if (strcmp(proc_movie_colorspace, "rgb") == 0)
        {
            game_log("The libx264 encoder can only use YUV color spaces\n");
            return false;
        }
    }

    else if (strcmp(proc_movie_encoder, "libx264rgb") == 0)
    {
        if (strcmp(proc_movie_pxformat, "bgr0") != 0)
        {
            game_log("The libx264rgb encoder can only use the rgb pixel format\n");
            return false;
        }

        if (strcmp(proc_movie_colorspace, "rgb") != 0)
        {
            game_log("The libx264rgb encoder can only use the rgb color space\n");
            return false;
        }
    }

    if (proc_mosample_mult == 1)
    {
        game_log("motion_blur_fps_mult is set to 1, which doesn't enable motion blur\n");
        return false;
    }

    return true;
}

void set_default_profile()
{
    proc_movie_encode_threads = 0;
    proc_movie_fps = 60;
    proc_movie_encoder = "libx264";
    proc_movie_pxformat = "yuv420";
    proc_movie_colorspace = "601";
    proc_movie_crf = 23;
    proc_movie_x264_preset = "veryfast";
    proc_movie_x264_intra = 0;
    proc_mosample_enabled = 1;
    proc_mosample_mult = 60;
    proc_mosample_exposure = 0.5f;
    proc_veloc_enabled = 0;
    StringCchCopyA(proc_veloc_font, MAX_VELOC_FONT_NAME, "Arial");
    proc_veloc_font_size = 72;
    proc_veloc_font_color[0] = 255;
    proc_veloc_font_color[1] = 255;
    proc_veloc_font_color[2] = 255;
    proc_veloc_font_color[3] = 255;
    proc_veloc_font_style = DWRITE_FONT_STYLE_NORMAL;
    proc_veloc_font_weight = DWRITE_FONT_WEIGHT_BOLD;
    proc_veloc_font_stretch = DWRITE_FONT_STRETCH_NORMAL;
    proc_veloc_text_align = DWRITE_TEXT_ALIGNMENT_CENTER;
    proc_veloc_para_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    proc_veloc_padding = 100;
}

void log_profile()
{
    game_log("Using profile:\n");
    game_log("Encoding threads: %d\n", proc_movie_encode_threads);
    game_log("Movie fps: %d\n", proc_movie_fps);
    game_log("Video encoder: %s\n", proc_movie_encoder);
    game_log("Video pixel format: %s\n", proc_movie_pxformat);
    game_log("Video colorspace: %s\n", proc_movie_colorspace);
    game_log("Video crf: %d\n", proc_movie_crf);
    game_log("Video x264 preset: %s\n", proc_movie_x264_preset);
    game_log("Video x264 intra: %d\n", proc_movie_x264_intra);
    game_log("Use motion blur: %d\n", proc_mosample_enabled);

    if (proc_mosample_enabled)
    {
        game_log("Motion blur multiplier: %d\n", proc_mosample_mult);
        game_log("Motion blur exposure: %0.2f\n", proc_mosample_exposure);
    }

    game_log("Use velocity overlay: %d\n", proc_veloc_enabled);

    if (proc_veloc_enabled)
    {
        game_log("Velocity font: %s\n", proc_veloc_font);
        game_log("Velocity font size: %d\n", proc_veloc_font_size);
        game_log("Velocity font color: %d %d %d %d\n", proc_veloc_font_color[0], proc_veloc_font_color[1], proc_veloc_font_color[2], proc_veloc_font_color[3]);
        game_log("Velocity font style: %s\n", rl_map_str_in_list(proc_veloc_font_style, FONT_STYLE_TABLE, SVR_ARRAY_SIZE(FONT_STYLE_TABLE)));
        game_log("Velocity font weight: %s\n", rl_map_str_in_list(proc_veloc_font_weight, FONT_WEIGHT_TABLE, SVR_ARRAY_SIZE(FONT_WEIGHT_TABLE)));
        game_log("Velocity font stretch: %s\n", rl_map_str_in_list(proc_veloc_font_stretch, FONT_STRETCH_TABLE, SVR_ARRAY_SIZE(FONT_STRETCH_TABLE)));
        game_log("Velocity text align: %s\n", rl_map_str_in_list(proc_veloc_text_align, TEXT_ALIGN_TABLE, SVR_ARRAY_SIZE(TEXT_ALIGN_TABLE)));
        game_log("Velocity paragraph align: %s\n", rl_map_str_in_list(proc_veloc_para_align, PARAGRAPH_ALIGN_TABLE, SVR_ARRAY_SIZE(PARAGRAPH_ALIGN_TABLE)));
        game_log("Velocity text padding: %d\n", proc_veloc_padding);
    }
}

// We start a separate process for encoding for two reasons:
// 1) Source is a 32-bit engine, and it was common to run out of memory in games such as CSGO that uses a lot of memory.
// 2) The ffmpeg api is horrible to work with with an incredible amount of pitfalls that will grant you a media that is slighly not correct
//    and there is no reliable documentation.
// Data is sent to this process through a pipe that we create.
void start_ffmpeg_proc()
{
    extern const char* svr_resource_path;

    char full_ffmpeg_path[MAX_PATH];
    full_ffmpeg_path[0] = 0;
    StringCchCatA(full_ffmpeg_path, MAX_PATH, svr_resource_path);
    StringCchCatA(full_ffmpeg_path, MAX_PATH, "\\ffmpeg.exe");

    const s32 FULL_ARGS_SIZE = 512;
    const s32 ARGS_BUF_SIZE = 64;

    char full_args[FULL_ARGS_SIZE];
    full_args[0] = 0;

    char buf[ARGS_BUF_SIZE];

    StringCchCatA(full_args, FULL_ARGS_SIZE, "-hide_banner");

    #if SVR_RELEASE
    StringCchCatA(full_args, FULL_ARGS_SIZE, " -loglevel quiet");
    #else
    StringCchCatA(full_args, FULL_ARGS_SIZE, " -loglevel debug");
    #endif

    PxConvText pxconv_text = PXCONV_FFMPEG_TEXT_TABLE[proc_movie_pxconv];

    // We are sending raw uncompressed data.
    StringCchCatA(full_args, FULL_ARGS_SIZE, " -f rawvideo -vcodec rawvideo");

    // Video size.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -s %dx%d", proc_movie_width, proc_movie_height);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Pixel format that goes through the pipe.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -pix_fmt %s", pxconv_text.format);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Input frame rate.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -r %d", proc_movie_fps);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Overwrite existing, and read from stdin.
    StringCchCatA(full_args, FULL_ARGS_SIZE, " -y -i -");

    // Number of encoding threads, or 0 for auto.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -threads %d", proc_movie_encode_threads);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Output video codec.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -vcodec %s", proc_movie_encoder);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    if (pxconv_text.color_space)
    {
        // Output video color space (only for YUV).
        StringCchPrintfA(buf, ARGS_BUF_SIZE, " -colorspace %s", pxconv_text.color_space);
        StringCchCatA(full_args, FULL_ARGS_SIZE, buf);
    }

    // Output video framerate.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -framerate %d", proc_movie_fps);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Output quality factor.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -crf %d", proc_movie_crf);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    // Output x264 preset.
    StringCchPrintfA(buf, ARGS_BUF_SIZE, " -preset %s", proc_movie_x264_preset);
    StringCchCatA(full_args, FULL_ARGS_SIZE, buf);

    if (proc_movie_x264_intra)
    {
        StringCchCatA(full_args, FULL_ARGS_SIZE, " -x264-params keyint=1");
    }

    // The path can be specified as relative here because we set the working directory of the ffmpeg process
    // to the SVR directory.

    StringCchCatA(full_args, FULL_ARGS_SIZE, " \"");
    StringCchCatA(full_args, FULL_ARGS_SIZE, proc_movie_path);
    StringCchCatA(full_args, FULL_ARGS_SIZE, "\"");

    HANDLE read_h;
    HANDLE write_h;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    CreatePipe(&read_h, &write_h, &sa, 4 * 1024);

    // Since we start the ffmpeg process with inherited handles, it would try to inherit the writing endpoint of this pipe too.
    // We have to remove the inheritance of this pipe. Otherwise it's never able to exit.
    SetHandleInformation(write_h, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = read_h;
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    DWORD create_proc_flags = 0;

    #if SVR_RELEASE
    create_proc_flags |= CREATE_NO_WINDOW;
    #endif

    PROCESS_INFORMATION info;
    BOOL res = CreateProcessA(full_ffmpeg_path, full_args, NULL, NULL, TRUE, create_proc_flags, NULL, svr_resource_path, &start_info, &info);

    if (res == 0)
    {
        game_log("Could not create ffmpeg process (%lu)\n", GetLastError());
    }

    proc_ffmpeg_proc = info.hProcess;
    CloseHandle(info.hThread);

    CloseHandle(read_h);

    proc_ffmpeg_write_pipe = write_h;
}

void end_ffmpeg_proc()
{
    while (proc_ffmpeg_write_queue.read_buffer_health() > 0)
    {
        // Stupid but we can't use any other mechanism with the ffmpeg thread loop.
        Sleep(100);
    }

    // Both queues should not be updated anymore at this point so they should be the same when observed.
    assert(proc_ffmpeg_write_queue.read_buffer_health() == 0);
    assert(proc_ffmpeg_read_queue.read_buffer_health() == MAX_BUFFERED_SEND_FRAMES);

    // Close our end of the pipe.
    // This will mark the completion of the stream, and the process will finish its work.
    CloseHandle(proc_ffmpeg_write_pipe);
    proc_ffmpeg_write_pipe = NULL;

    WaitForSingleObject(proc_ffmpeg_proc, INFINITE);

    CloseHandle(proc_ffmpeg_proc);
    proc_ffmpeg_proc = NULL;
}

void proc_start(const char* dest, const char* profile, void* game_share_h)
{
    if (*profile == 0)
    {
        profile = "default";
    }

    game_log("Starting movie to %s with profile %s\n", dest, profile);

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

    log_profile();

    for (s32 i = 0; i < NUM_PXCONVS; i++)
    {
        PxConvText& text = PXCONV_INI_TEXT_TABLE[i];

        if (strcmp(text.format, proc_movie_pxformat) == 0 && strcmp(text.color_space, proc_movie_colorspace) == 0)
        {
            proc_movie_pxconv = (PxConv)i;
            break;
        }
    }

    ID3D11Resource* temp_resource = NULL;
    proc_d3d11_device->OpenSharedResource((HANDLE)game_share_h, IID_PPV_ARGS(&temp_resource));

    temp_resource->QueryInterface(IID_PPV_ARGS(&proc_game_content_tex));
    temp_resource->Release();

    proc_d3d11_device->CreateShaderResourceView(proc_game_content_tex, NULL, &proc_game_content_srv);

    D3D11_TEXTURE2D_DESC tex_desc;
    proc_game_content_tex->GetDesc(&tex_desc);

    // Make the work texture start blank.

    proc_frame_num = 0;
    proc_movie_width = tex_desc.Width;
    proc_movie_height = tex_desc.Height;
    StringCchCopyA(proc_movie_path, MAX_PATH, dest);

    game_log("Video size: %dx%d\n", proc_movie_width, proc_movie_height);

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

        proc_d3d11_device->CreateTexture2D(&work_tex_desc, NULL, &proc_work_tex);
        proc_d3d11_device->CreateShaderResourceView(proc_work_tex, NULL, &proc_work_tex_srv);
        proc_d3d11_device->CreateRenderTargetView(proc_work_tex, NULL, &proc_work_tex_rtv);
        proc_d3d11_device->CreateUnorderedAccessView(proc_work_tex, NULL, &proc_work_tex_uav);
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

        proc_d3d11_device->CreateBuffer(&work_buf_desc, NULL, &proc_work_buf_legacy_sb);
        proc_d3d11_device->CreateShaderResourceView(proc_work_buf_legacy_sb, NULL, &proc_work_buf_legacy_sb_srv);
        proc_d3d11_device->CreateUnorderedAccessView(proc_work_buf_legacy_sb, NULL, &proc_work_buf_legacy_sb_uav);

        // For dest_texture_width.
        update_constant_buffer(proc_mosample_legacy_cb, &tex_desc.Width, sizeof(UINT));
    }

    proc_used_pxconv_planes = calc_format_planes(proc_movie_pxconv);

    switch (proc_movie_pxconv)
    {
        case PXCONV_YUV420_601:
        case PXCONV_YUV420_709:
        case PXCONV_YUV444_601:
        case PXCONV_YUV444_709:
        {
            for (s32 i = 0; i < 3; i++)
            {
                s32 dims[2];
                calc_plane_dims(proc_movie_pxconv, proc_movie_width, proc_movie_height, i, &dims[0], &dims[1]);

                D3D11_TEXTURE2D_DESC pxconv_tex_desc = {};
                pxconv_tex_desc.Width = dims[0];
                pxconv_tex_desc.Height = dims[1];
                pxconv_tex_desc.MipLevels = 1;
                pxconv_tex_desc.ArraySize = 1;
                pxconv_tex_desc.Format = DXGI_FORMAT_R8_UINT;
                pxconv_tex_desc.SampleDesc.Count = 1;
                pxconv_tex_desc.Usage = D3D11_USAGE_DEFAULT;
                pxconv_tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
                pxconv_tex_desc.CPUAccessFlags = 0;

                proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_texs[i]);
                proc_d3d11_device->CreateUnorderedAccessView(proc_pxconv_texs[i], NULL, &proc_pxconv_uavs[i]);

                pxconv_tex_desc.Usage = D3D11_USAGE_STAGING;
                pxconv_tex_desc.BindFlags = 0;
                pxconv_tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

                for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
                {
                    proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_dls[i].texs[j]);
                }
            }

            break;
        }

        case PXCONV_NV12_601:
        case PXCONV_NV21_601:
        case PXCONV_NV12_709:
        case PXCONV_NV21_709:
        {
            for (s32 i = 0; i < 2; i++)
            {
                s32 dims[2];
                calc_plane_dims(proc_movie_pxconv, proc_movie_width, proc_movie_height, i, &dims[0], &dims[1]);

                D3D11_TEXTURE2D_DESC pxconv_tex_desc = {};
                pxconv_tex_desc.Width = dims[0];
                pxconv_tex_desc.Height = dims[1];
                pxconv_tex_desc.MipLevels = 1;
                pxconv_tex_desc.ArraySize = 1;
                pxconv_tex_desc.Format = i == 0 ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R8G8_UINT;
                pxconv_tex_desc.SampleDesc.Count = 1;
                pxconv_tex_desc.Usage = D3D11_USAGE_DEFAULT;
                pxconv_tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
                pxconv_tex_desc.CPUAccessFlags = 0;

                proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_texs[i]);
                proc_d3d11_device->CreateUnorderedAccessView(proc_pxconv_texs[i], NULL, &proc_pxconv_uavs[i]);

                pxconv_tex_desc.Usage = D3D11_USAGE_STAGING;
                pxconv_tex_desc.BindFlags = 0;
                pxconv_tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

                for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
                {
                    proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_dls[i].texs[j]);
                }
            }

            break;
        }

        case PXCONV_BGR0:
        {
            D3D11_TEXTURE2D_DESC pxconv_tex_desc = {};
            pxconv_tex_desc.Width = proc_movie_width;
            pxconv_tex_desc.Height = proc_movie_height;
            pxconv_tex_desc.MipLevels = 1;
            pxconv_tex_desc.ArraySize = 1;
            pxconv_tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
            pxconv_tex_desc.SampleDesc.Count = 1;
            pxconv_tex_desc.Usage = D3D11_USAGE_DEFAULT;
            pxconv_tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            pxconv_tex_desc.CPUAccessFlags = 0;

            proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_texs[0]);
            proc_d3d11_device->CreateUnorderedAccessView(proc_pxconv_texs[0], NULL, &proc_pxconv_uavs[0]);

            pxconv_tex_desc.Usage = D3D11_USAGE_STAGING;
            pxconv_tex_desc.BindFlags = 0;
            pxconv_tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
            {
                proc_d3d11_device->CreateTexture2D(&pxconv_tex_desc, NULL, &proc_pxconv_dls[0].texs[j]);
            }

            break;
        }
    }

    for (s32 i = 0; i < proc_used_pxconv_planes; i++)
    {
        D3D11_TEXTURE2D_DESC tex_desc;
        proc_pxconv_texs[i]->GetDesc(&tex_desc);

        proc_pxconv_plane_sizes[i] = tex_desc.Width * tex_desc.Height * calc_bytes_pitch(tex_desc.Format);
        proc_pxconv_widths[i] = tex_desc.Width;
        proc_pxconv_heights[i] = tex_desc.Height;
        proc_pxconv_pitches[i] = tex_desc.Width * calc_bytes_pitch(tex_desc.Format);
    }

    // Combined size of all planes.
    proc_pxconv_total_plane_sizes = 0;

    for (s32 i = 0; i < proc_used_pxconv_planes; i++)
    {
        proc_pxconv_total_plane_sizes += proc_pxconv_plane_sizes[i];
    }

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
    {
        proc_ffmpeg_send_bufs[i] = (u8*)malloc(proc_pxconv_total_plane_sizes);
    }

    // Need to overwrite with new data.
    proc_ffmpeg_read_queue.reset();

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
    {
        proc_ffmpeg_read_queue.push(proc_ffmpeg_send_bufs[i]);
    }

    if (proc_mosample_enabled)
    {
        proc_mosample_remainder = 0.0f;

        s32 sps = proc_movie_fps * proc_mosample_mult;
        proc_mosample_remainder_step = (1.0f / sps) / (1.0f / proc_movie_fps);
    }

    start_ffmpeg_proc();
}

void send_converted_video_frame_to_ffmpeg()
{
    svr_sem_wait(&proc_ffmpeg_read_sem);

    u8* mem;
    bool res1 = proc_ffmpeg_read_queue.pull(&mem);
    assert(res1);

    svr_start_prof(dl_prof);
    download_textures(proc_pxconv_texs, proc_pxconv_dls, proc_used_pxconv_planes, mem, proc_pxconv_total_plane_sizes);
    svr_end_prof(dl_prof);

    proc_ffmpeg_write_queue.push(mem);

    svr_sem_release(&proc_ffmpeg_write_sem);
}

void motion_sample(float weight)
{
    if (weight != proc_mosample_weight_cache)
    {
        proc_mosample_weight_cache = weight;
        update_constant_buffer(proc_mosample_cb, &weight, sizeof(float));
    }

    // if (hw_has_typed_uav_support)
    {
        proc_d3d11_context->CSSetShader(proc_mosample_cs, NULL, 0);
        proc_d3d11_context->CSSetConstantBuffers(0, 1, &proc_mosample_cb);
        proc_d3d11_context->CSSetUnorderedAccessViews(0, 1, &proc_work_tex_uav, NULL);
        proc_d3d11_context->CSSetShaderResources(0, 1, &proc_game_content_srv);
    }

    // else
    // {
    //     ID3D11Buffer* legacy_cbs[] = { proc_mosample_cb, proc_mosample_legacy_cb };
    //     proc_d3d11_context->CSSetShader(proc_mosample_legacy_cs, NULL, 0);
    //     proc_d3d11_context->CSSetConstantBuffers(0, 2, legacy_cbs);
    //     proc_d3d11_context->CSSetUnorderedAccessViews(0, 1, &proc_work_buf_legacy_sb_uav, NULL);
    //     proc_d3d11_context->CSSetShaderResources(0, 1, &proc_game_content_srv);
    // }

    proc_d3d11_context->Dispatch(calc_cs_thread_groups(proc_movie_width), calc_cs_thread_groups(proc_movie_height), 1);

    svr_start_prof(mosample_prof);
    proc_d3d11_context->Flush();
    svr_end_prof(mosample_prof);

    ID3D11ShaderResourceView* null_srvs[] = { NULL };
    ID3D11UnorderedAccessView* null_uavs[] = { NULL };

    proc_d3d11_context->CSSetShaderResources(0, 1, null_srvs);
    proc_d3d11_context->CSSetUnorderedAccessViews(0, 1, null_uavs, NULL);
}

void encode_video_frame(ID3D11ShaderResourceView* srv)
{
    convert_pixel_formats(srv);
    send_converted_video_frame_to_ffmpeg();
}

void proc_frame()
{
    if (proc_frame_num > 0)
    {
        svr_start_prof(frame_prof);

        if (proc_mosample_enabled)
        {
            float old_rem = proc_mosample_remainder;
            float exposure = proc_mosample_exposure;

            proc_mosample_remainder += proc_mosample_remainder_step;

            if (proc_mosample_remainder <= (1.0f - exposure))
            {

            }

            else if (proc_mosample_remainder < 1.0f)
            {
                float weight = (proc_mosample_remainder - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);
                motion_sample(weight);
            }

            else
            {
                float weight = (1.0f - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);
                motion_sample(weight);

                encode_video_frame(proc_work_tex_srv);

                proc_mosample_remainder -= 1.0f;

                s32 additional = proc_mosample_remainder;

                if (additional > 0)
                {
                    for (s32 i = 0; i < additional; i++)
                    {
                        encode_video_frame(proc_work_tex_srv);
                    }

                    proc_mosample_remainder -= additional;
                }

                float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                proc_d3d11_context->ClearRenderTargetView(proc_work_tex_rtv, clear_color);

                if (proc_mosample_remainder > FLT_EPSILON && proc_mosample_remainder > (1.0f - exposure))
                {
                    weight = ((proc_mosample_remainder - (1.0f - exposure)) * (1.0f / exposure));
                    motion_sample(weight);
                }
            }
        }

        else
        {
            encode_video_frame(proc_game_content_srv);
        }

        svr_end_prof(frame_prof);
    }

    proc_frame_num++;
}

void show_total_prof(const char* name, SvrProf& prof)
{
    game_log("%s: %lld\n", name, prof.total);
}

void show_prof(const char* name, SvrProf& prof)
{
    if (prof.runs > 0)
    {
        game_log("%s: %lld\n", name, prof.total / prof.runs);
    }
}

void proc_end()
{
    game_log("Ending movie\n");

    end_ffmpeg_proc();

    for (s32 i = 0; i < MAX_BUFFERED_SEND_FRAMES; i++)
    {
        free(proc_ffmpeg_send_bufs[i]);
    }

    proc_game_content_tex->Release();
    proc_game_content_srv->Release();

    if (hw_has_typed_uav_support)
    {
        proc_work_tex->Release();
        proc_work_tex_rtv->Release();
        proc_work_tex_srv->Release();
        proc_work_tex_uav->Release();
    }

    else
    {
        proc_work_buf_legacy_sb->Release();
        proc_work_buf_legacy_sb_srv->Release();
        proc_work_buf_legacy_sb_uav->Release();
    }

    for (s32 i = 0; i < proc_used_pxconv_planes; i++)
    {
        proc_pxconv_texs[i]->Release();

        for (s32 j = 0; j < NUM_BUFFERED_DL_TEXS; j++)
        {
            proc_pxconv_dls[i].texs[j]->Release();
        }

        proc_pxconv_uavs[i]->Release();
    }

    #if SVR_PROF
    show_total_prof("Total work time", frame_prof);
    show_prof("Download", dl_prof);
    show_prof("Write", write_prof);
    show_prof("Mosample", mosample_prof);
    #endif

    svr_reset_prof(frame_prof);
    svr_reset_prof(dl_prof);
    svr_reset_prof(write_prof);
    svr_reset_prof(mosample_prof);

    game_log("Movie finished\n");
}

s32 proc_get_game_rate()
{
    if (proc_mosample_enabled)
    {
        return proc_movie_fps * proc_mosample_mult;
    }

    return proc_movie_fps;
}
