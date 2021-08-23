#include "game_proc_nvenc.h"
#include "svr_logging.h"
#include <d3d11_4.h>
#include <nvEncodeAPI.h>
#include "svr_sem.h"
#include "svr_stream.h"
#include <assert.h>
#include <dxgi1_6.h>

// NVENC encoding for NVIDIA cards.
// Implemented with version 11.0.10 of the SDK.
// See the documentation in NVENC_VideoEncoder_API_ProgGuide.pdf (from NVIDIA website).

// NV_ENC_CAPS_SUPPORT_YUV444_ENCODE
// NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE

const UINT NVIDIA_VENDOR_ID = 0x10de;

// This device and context are used in the NVENC thread that receives the encoded output.
// We pass this to NVENC for its processing for multithreaded purposes since ID3D11DeviceContext is not thread safe (so we cannot
// use the existing nvenc_d3d11_device).
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

void proc_init_nvenc()
{
    UINT device_create_flags = 0;

    #if SVR_DEBUG
    device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    // Should be good enough for all the features that we make use of.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_VERSION = D3D_FEATURE_LEVEL_11_0;

    const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
        MINIMUM_DEVICE_VERSION
    };

    D3D_FEATURE_LEVEL created_device_level;
    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &nvenc_d3d11_device, &created_device_level, &nvenc_d3d11_context);

    NVENCSTATUS ns;

    nvenc_funs.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    ns = NvEncodeAPICreateInstance(&nvenc_funs);

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS nvenc_open_session_params = {};
    nvenc_open_session_params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    nvenc_open_session_params.device = nvenc_d3d11_device;
    nvenc_open_session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    nvenc_open_session_params.apiVersion = NVENCAPI_VERSION;
    ns = nvenc_funs.nvEncOpenEncodeSessionEx(&nvenc_open_session_params, &nvenc_encoder);

    bool has_lookahead = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_LOOKAHEAD);
    bool has_psycho_aq = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ);
    bool has_lossless_encode = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE);
    bool has_444_encode = check_nvenc_support_cap(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE);

    svr_log("NVENC lookahead support: %d\n", (s32)has_lookahead);
    svr_log("NVENC psycho aq support: %d\n", (s32)has_psycho_aq);
    svr_log("NVENC lossless encode support: %d\n", (s32)has_lossless_encode);
    svr_log("NVENC 444 encode support: %d\n", (s32)has_444_encode);
}

void proc_start_nvenc(s32 width, s32 height, s32 fps)
{
    NVENCSTATUS ns;

    GUID preset_guid = NV_ENC_PRESET_LOSSLESS_HP_GUID;

    NV_ENC_INITIALIZE_PARAMS nvenc_init_params = {};
    nvenc_init_params.version = NV_ENC_INITIALIZE_PARAMS_VER;
    nvenc_init_params.encodeGUID = NV_ENC_CODEC_H264_GUID;
    nvenc_init_params.presetGUID = preset_guid;
    nvenc_init_params.encodeWidth = width;
    nvenc_init_params.encodeHeight = height;
    nvenc_init_params.frameRateNum = fps;
    nvenc_init_params.frameRateDen = 1;
    nvenc_init_params.enableEncodeAsync = 1;
    nvenc_init_params.enablePTD = 1;
    ns = nvenc_funs.nvEncInitializeEncoder(nvenc_encoder, &nvenc_init_params);

    NV_ENC_PRESET_CONFIG preset_config;
    preset_config.version = NV_ENC_PRESET_CONFIG_VER;
    preset_config.presetCfg.version = NV_ENC_CONFIG_VER;
    ns = nvenc_funs.nvEncGetEncodePresetConfig(nvenc_encoder, NV_ENC_CODEC_H264_GUID, preset_guid, &preset_config);

    // Note: The client should allocate at least (1 + NB) input and output buffers, where NB is the
    // number of B frames between successive P frames.
    nvenc_num_pics = svr_max(4, preset_config.presetCfg.frameIntervalP * 2 * 2);

    for (s32 i = 0; i < nvenc_num_pics; i++)
    {
        D3D11_TEXTURE2D_DESC enc_tex_desc = {};
        enc_tex_desc.Width = width;
        enc_tex_desc.Height = height;
        enc_tex_desc.MipLevels = 1;
        enc_tex_desc.ArraySize = 1;
        enc_tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        enc_tex_desc.SampleDesc.Count = 1;
        enc_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        enc_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        ID3D11Texture2D* tex;
        nvenc_d3d11_device->CreateTexture2D(&enc_tex_desc, NULL, &tex);

        tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

        NV_ENC_REGISTER_RESOURCE nvenc_resource = {};
        nvenc_resource.version = NV_ENC_REGISTER_RESOURCE_VER;
        nvenc_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        nvenc_resource.width = width;
        nvenc_resource.height = height;
        nvenc_resource.resourceToRegister = tex;
        nvenc_resource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
        nvenc_resource.bufferUsage = NV_ENC_INPUT_IMAGE;

        ns = nvenc_funs.nvEncRegisterResource(nvenc_encoder, &nvenc_resource);

        // Output of above function is placed here.
        nvenc_resource.registeredResource;
    }
}

void proc_nvenc_frame()
{

}

void proc_stop_nvenc()
{

}

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
    CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi_factory));

    IDXGIAdapter1* adapter = NULL;
    bool supported = false;

    for (UINT i = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i, &adapter)); i++)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        adapter->Release();

        // Integrated chips are not supported.
        if (desc.VendorId == NVIDIA_VENDOR_ID && !is_blacklisted_device(desc.DeviceId))
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

bool proc_is_nvenc_supported_real()
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
    static bool ret = proc_is_nvenc_supported_real();
    return ret;
}
