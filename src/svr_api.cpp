#include "svr_api.h"
#include "game_proc.h"
#include "game_proc_nvenc.h"
#include <d3d11.h>
#include <d3d9.h>
#include "game_shared.h"
#include <strsafe.h>

// Used for internal and external SVR.
// This layer if necessary translates operations from D3D9Ex to D3D11 which game_proc uses and is also the public API.

ID3D11Device* svr_d3d11_device;
ID3D11DeviceContext* svr_d3d11_context;

// For D3D9Ex, we have to create an additional D3D9Ex texture and then copy that to a D3D11 texture.
IDirect3DDevice9Ex* svr_d3d9ex_device;
IDirect3DSurface9* svr_d3d9ex_content_surf;
HANDLE svr_d3d9ex_share_h;
IDirect3DTexture9* svr_d3d9ex_share_tex;
IDirect3DSurface9* svr_d3d9ex_share_surf;

// For D3D11 we can read the game texture directly.
// Destination texture that we work with (Both D3D11 and D3D9Ex).
ID3D11Texture2D* svr_content_tex;
ID3D11ShaderResourceView* svr_content_srv;

bool svr_movie_running;

int svr_api_version()
{
    return SVR_API_VERSION;
}

int svr_dll_version()
{
    return SVR_VERSION;
}

bool svr_can_use_nvenc()
{
    return proc_is_nvenc_supported();
}

bool svr_init(const char* svr_path, IUnknown* game_device)
{
    ID3D11Device* game_d3d11_device = NULL;
    IDirect3DDevice9Ex* game_d3d9ex_device = NULL;

    game_device->QueryInterface(IID_PPV_ARGS(&game_d3d11_device));
    game_device->QueryInterface(IID_PPV_ARGS(&game_d3d9ex_device));

    if (game_d3d11_device == NULL && game_d3d9ex_device == NULL)
    {
        OutputDebugStringA("SVR (svr_init): The passed game_device is not a D3D11 (ID3D11Device) or a D3D9Ex (IDirect3DDevice9Ex) type\n");
        return false;
    }

    if (game_d3d11_device)
    {
        UINT device_flags = game_d3d11_device->GetCreationFlags();
        UINT device_level = game_d3d11_device->GetFeatureLevel();

        if (device_level < (UINT)D3D_FEATURE_LEVEL_11_0)
        {
            game_d3d11_device->Release();

            OutputDebugStringA("SVR (svr_init): The game D3D11 device must be created with D3D_FEATURE_LEVEL_11_0 or higher\n");
            return false;
        }

        if (!(device_flags & D3D11_CREATE_DEVICE_BGRA_SUPPORT))
        {
            game_d3d11_device->Release();

            OutputDebugStringA("SVR (svr_init): The game D3D11 device must be created with the D3D11_CREATE_DEVICE_BGRA_SUPPORT flag\n");
            return false;
        }

        #if SVR_DEBUG
        if (device_flags & D3D11_CREATE_DEVICE_DEBUG)
        {
            OutputDebugStringA("SVR (svr_init): The game D3D11 device has the debug layer enabled\n");
        }
        #endif

        game_d3d11_device->GetImmediateContext(&svr_d3d11_context);
    }

    // If we are a D3D9Ex game, we have to create a new D3D11 device for game_proc.
    else if (game_d3d9ex_device)
    {
        // BGRA support needed for Direct2D interoperability.
        // It is also only intended to be used from a single thread.
        UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        #if SVR_DEBUG
        device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        // Should be good enough for all the features that we make use of.
        const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_11_0;

        const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
            MINIMUM_DEVICE_LEVEL
        };

        D3D_FEATURE_LEVEL created_device_level;
        D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &svr_d3d11_device, &created_device_level, &svr_d3d11_context);
    }

    game_init();

    bool ret = proc_init(svr_path, svr_d3d11_device);

    if (!ret)
    {

    }

    return ret;
}

bool svr_movie_active()
{
    return svr_movie_running;
}

bool svr_start(const char* movie_name, const char* movie_profile, SvrStartMovie* movie_data)
{
    if (svr_movie_running)
    {
        OutputDebugStringA("SVR (svr_start): Movie is already started. It is not allowed to call this now\n");
        return false;
    }

    movie_data->game_tex_view->QueryInterface(IID_PPV_ARGS(&svr_d3d9ex_content_surf));
    movie_data->game_tex_view->QueryInterface(IID_PPV_ARGS(&svr_content_srv));

    if (svr_d3d9ex_content_surf == NULL && svr_content_srv == NULL)
    {
        OutputDebugStringA("SVR (svr_start): The passed game texture view is not a D3D11 (ID3D11ShaderResourceView) or a D3D9Ex (IDirect3DSurface9) type\n");
        return false;
    }

    // We are a D3D11 game.
    if (svr_content_srv)
    {
        // We can get the texture from the srv, and then get the dimensions of the texture.

        ID3D11Resource* game_tex_res;
        svr_content_srv->GetResource(&game_tex_res);

        game_tex_res->QueryInterface(IID_PPV_ARGS(&svr_content_tex));

        D3D11_TEXTURE2D_DESC game_tex_desc;
        svr_content_tex->GetDesc(&game_tex_desc);

        game_tex_res->Release();
    }

    // We are a D3D9Ex game.
    else if (svr_d3d9ex_content_surf)
    {
        // Have to create a new texture that we can open in D3D11. This D3D9Ex texture will be copied to from the
        // game content texture.

        D3DSURFACE_DESC desc;
        svr_d3d9ex_content_surf->GetDesc(&desc);

        svr_d3d9ex_device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &svr_d3d9ex_share_tex, &svr_d3d9ex_share_h);
        svr_d3d9ex_share_tex->GetSurfaceLevel(0, &svr_d3d9ex_share_surf);

        ID3D11Resource* temp_resource = NULL;
        svr_d3d11_device->OpenSharedResource(svr_d3d9ex_share_h, IID_PPV_ARGS(&temp_resource));

        temp_resource->QueryInterface(IID_PPV_ARGS(&svr_content_tex));
        temp_resource->Release();

        svr_d3d11_device->CreateShaderResourceView(svr_content_tex, NULL, &svr_content_srv);
    }

    bool ret = proc_start(svr_d3d11_device, svr_d3d11_context, movie_name, movie_profile, svr_content_srv);

    if (!ret)
    {

    }

    svr_movie_running = ret;

    return ret;
}

int svr_get_game_rate()
{
    if (!svr_movie_running)
    {
        OutputDebugStringA("SVR (svr_get_game_rate): Movie is not started. It is not allowed to call this now\n");
        return 0;
    }

    return proc_get_game_rate();
}

void svr_stop()
{
    if (!svr_movie_running)
    {
        OutputDebugStringA("SVR (svr_stop): Movie is not started. It is not allowed to call this now\n");
        return;
    }

    proc_end();

    if (svr_d3d9ex_device)
    {
        svr_d3d9ex_content_surf->Release();
        svr_d3d9ex_share_tex->Release();
        svr_d3d9ex_share_surf->Release();
    }

    svr_content_tex->Release();
    svr_content_srv->Release();

    svr_movie_running = false;
}

void svr_frame()
{
    if (!svr_movie_running)
    {
        return;
    }

    // If we are a D3D9Ex game, we have to copy over the game content texture to the D3D11 texture.
    if (svr_d3d9ex_device)
    {
        // Copy over the game content to the shared texture.
        // Don't use any filtering type because the source and destinations are both same size.
        svr_d3d9ex_device->StretchRect(svr_d3d9ex_content_surf, NULL, svr_d3d9ex_share_surf, NULL, D3DTEXF_NONE);
    }

    // The D3D11 texture now contains the game content.

    proc_frame(svr_d3d11_context, svr_content_srv);
}

void svr_give_velocity(float* xyz)
{
    proc_give_velocity(xyz);
}
