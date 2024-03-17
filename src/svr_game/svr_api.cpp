#include "proc_priv.h"

// Used for internal and external SVR.
// This layer if necessary translates operations from D3D9Ex to D3D11.

// -------------------------------------------------

ID3D11Device* svr_d3d11_device;
ID3D11DeviceContext* svr_d3d11_context;
IDirect3DDevice9Ex* svr_d3d9ex_device;

// For D3D9Ex, we have to create an additional D3D9Ex texture and then copy that to a D3D11 texture.
IDirect3DSurface9* svr_d3d9ex_content_surf;
IDirect3DSurface9* svr_d3d9ex_share_surf;

// For D3D11 we can read the game texture directly.
// Destination texture that we work with (Both D3D11 and D3D9Ex).
ID3D11Texture2D* svr_content_tex;
ID3D11ShaderResourceView* svr_content_srv;
ID3D11RenderTargetView* svr_content_rtv;

// -------------------------------------------------

ProcState proc_state;

// -------------------------------------------------

bool svr_movie_running;

// -------------------------------------------------

int svr_api_version()
{
    return SVR_API_VERSION;
}

int svr_dll_version()
{
    return SVR_VERSION;
}

// Stuff that is created during init.
void free_all_static_svr_stuff()
{
    svr_maybe_release(&svr_d3d11_device);
    svr_maybe_release(&svr_d3d11_context);
    svr_maybe_release(&svr_d3d9ex_device);
}

// Stuff that is created during movie start.
void free_all_dynamic_svr_stuff()
{
    svr_maybe_release(&svr_d3d9ex_content_surf);
    svr_maybe_release(&svr_d3d9ex_share_surf);
    svr_maybe_release(&svr_content_tex);
    svr_maybe_release(&svr_content_srv);
    svr_maybe_release(&svr_content_rtv);
}

bool svr_init(const char* svr_path, IUnknown* game_device)
{
    bool ret = false;

    // Adds a reference if successful.
    game_device->QueryInterface(IID_PPV_ARGS(&svr_d3d11_device));
    game_device->QueryInterface(IID_PPV_ARGS(&svr_d3d9ex_device));

    if (svr_d3d11_device == NULL && svr_d3d9ex_device == NULL)
    {
        OutputDebugStringA("SVR (svr_init): The passed game_device is not a D3D11 (ID3D11Device) or a D3D9Ex (IDirect3DDevice9Ex) type\n");
        goto rfail;
    }

    if (svr_d3d11_device)
    {
        svr_log("Init for a D3D11 game\n");

        UINT device_flags = svr_d3d11_device->GetCreationFlags();
        UINT device_level = svr_d3d11_device->GetFeatureLevel();

        if (device_level < (UINT)D3D_FEATURE_LEVEL_12_0)
        {
            OutputDebugStringA("SVR (svr_init): The game D3D11 device must be created with D3D_FEATURE_LEVEL_12_0 or higher\n");
            goto rfail;
        }

        if (!(device_flags & D3D11_CREATE_DEVICE_BGRA_SUPPORT))
        {
            OutputDebugStringA("SVR (svr_init): The game D3D11 device must be created with the D3D11_CREATE_DEVICE_BGRA_SUPPORT flag\n");
            goto rfail;
        }

        #if SVR_DEBUG
        if (device_flags & D3D11_CREATE_DEVICE_DEBUG)
        {
            OutputDebugStringA("SVR (svr_init): The game D3D11 device has the debug layer enabled\n");
        }
        #endif

        svr_d3d11_device->GetImmediateContext(&svr_d3d11_context);
    }

    // If we are a D3D9Ex game, we have to create a new D3D11 device for game_proc.
    else if (svr_d3d9ex_device)
    {
        svr_log("Init for a D3D9Ex game\n");

        // BGRA support needed for Direct2D interoperability.
        // It is also only intended to be used from a single thread.
        UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        #if SVR_DEBUG
        device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        // Should be good enough for all the features that we make use of.
        const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_12_0;

        const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
            MINIMUM_DEVICE_LEVEL
        };

        HRESULT hr;

        D3D_FEATURE_LEVEL created_device_level;

        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &svr_d3d11_device, &created_device_level, &svr_d3d11_context);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create D3D11 device (%#x)\n", hr);
            goto rfail;
        }
    }

    game_console_init();

    if (!proc_state.init(svr_path, svr_d3d11_device))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    free_all_static_svr_stuff();

rexit:
    return ret;
}

bool svr_movie_active()
{
    return svr_movie_running;
}

bool svr_start(const char* movie_name, const char* movie_profile, SvrStartMovieData* movie_data)
{
    bool ret = false;

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
        goto rfail;
    }

    // We are a D3D11 game.
    if (svr_content_srv)
    {
        ID3D11Resource* content_res = NULL;
        svr_content_srv->GetResource(&content_res);
        content_res->QueryInterface(IID_PPV_ARGS(&svr_content_tex));
        content_res->Release();

        D3D11_TEXTURE2D_DESC tex_desc;
        svr_content_tex->GetDesc(&tex_desc);

        if (!(tex_desc.BindFlags & D3D11_BIND_RENDER_TARGET))
        {
            OutputDebugStringA("SVR (svr_start): The passed game texture must be created with D3D11_BIND_RENDER_TARGET\n");
            goto rfail;
        }

        svr_d3d11_device->CreateRenderTargetView(svr_content_tex, NULL, &svr_content_rtv);
    }

    // We are a D3D9Ex game.
    else if (svr_d3d9ex_content_surf)
    {
        // Have to create a new texture that we can open in D3D11. This D3D9Ex texture will be copied to the game content texture every frame.

        D3DSURFACE_DESC desc;
        svr_d3d9ex_content_surf->GetDesc(&desc);

        HRESULT hr;
        HANDLE d3d9ex_share_h = NULL;
        IDirect3DTexture9* d3d9ex_share_tex = NULL;

        hr = svr_d3d9ex_device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &d3d9ex_share_tex, &d3d9ex_share_h);

        if (FAILED(hr))
        {
            svr_log("ERROR: Could not create D3D9Ex share texture (%#x)\n", hr);
            goto rfail;
        }

        d3d9ex_share_tex->GetSurfaceLevel(0, &svr_d3d9ex_share_surf);
        d3d9ex_share_tex->Release();

        svr_d3d11_device->OpenSharedResource(d3d9ex_share_h, IID_PPV_ARGS(&svr_content_tex));
        svr_d3d11_device->CreateShaderResourceView(svr_content_tex, NULL, &svr_content_srv);
        svr_d3d11_device->CreateRenderTargetView(svr_content_tex, NULL, &svr_content_rtv);
    }

    ProcGameTexture game_texture = {};
    game_texture.tex = svr_content_tex;
    game_texture.srv = svr_content_srv;
    game_texture.rtv = svr_content_rtv;

    if (!proc_state.start(movie_name, movie_profile, &game_texture))
    {
        goto rfail;
    }

    svr_movie_running = true;

    ret = true;
    goto rexit;

rfail:
    free_all_dynamic_svr_stuff();

rexit:
    return ret;
}

int svr_get_game_rate()
{
    if (!svr_movie_running)
    {
        OutputDebugStringA("SVR (svr_get_game_rate): Movie is not started. It is not allowed to call this now\n");
        return 0;
    }

    return proc_state.get_game_rate();
}

void svr_stop()
{
    if (!svr_movie_running)
    {
        OutputDebugStringA("SVR (svr_stop): Movie is not started. It is not allowed to call this now\n");
        return;
    }

    proc_state.end();

    free_all_dynamic_svr_stuff();

    svr_movie_running = false;
}

void svr_frame()
{
    // If we are a D3D9Ex game, we have to copy over the game content texture to the D3D11 texture.
    if (svr_d3d9ex_device)
    {
        // Copy over the game content to the shared texture.
        // Don't use any filtering type because the source and destinations are both same size.
        svr_d3d9ex_device->StretchRect(svr_d3d9ex_content_surf, NULL, svr_d3d9ex_share_surf, NULL, D3DTEXF_NONE);
    }

    // The D3D11 texture now contains the game content.

    proc_state.new_video_frame();
}

bool svr_is_velo_enabled()
{
    return proc_state.is_velo_enabled();
}

bool svr_is_audio_enabled()
{
    return proc_state.is_audio_enabled();
}

void svr_give_velocity(float* xyz)
{
    proc_state.velo_give(xyz);
}

void svr_give_audio(SvrWaveSample* samples, int num_samples)
{
    proc_state.new_audio_samples(samples, num_samples);
}
