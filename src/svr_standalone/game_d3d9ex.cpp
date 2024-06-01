#include "game_priv.h"

struct GameD3D9ExState
{
    IDirect3DDevice9Ex* device;

    GameFnHook present_hook;
};

GameD3D9ExState game_d3d9ex_state;

HRESULT __stdcall game_d3d9ex_present_override(void* p, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    if (game_state.rec_disable_window_update)
    {
        if (svr_movie_active())
        {
            return S_OK;
        }
    }

    using OrgFn = decltype(game_d3d9ex_present_override)*;
    OrgFn org_fn = (OrgFn)game_d3d9ex_state.present_hook.original;
    return org_fn(p, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

void game_d3d9ex_init()
{
    game_d3d9ex_state.device = (IDirect3DDevice9Ex*)game_get_d3d9ex_device();
    game_d3d9ex_state.device->AddRef();

    // Fixed in ABI and cannot be changed.
    GameFnOverride d3d9ex_present_override;
    d3d9ex_present_override.target = game_get_virtual(game_get_d3d9ex_device(), 17);
    d3d9ex_present_override.override = game_d3d9ex_present_override;

    game_hook_create(&d3d9ex_present_override, &game_d3d9ex_state.present_hook);
}

void game_d3d9ex_free()
{
    game_d3d9ex_state.device->Release();
    game_d3d9ex_state.device = NULL;

    game_hook_remove(&game_d3d9ex_state.present_hook);
}

IUnknown* game_d3d9ex_get_game_device()
{
    game_d3d9ex_state.device->AddRef();
    return game_d3d9ex_state.device;
}

IUnknown* game_d3d9ex_get_game_texture()
{
    // The game backbuffer is the first index.
    IDirect3DSurface9* bb_surf = NULL;
    game_d3d9ex_state.device->GetRenderTarget(0, &bb_surf);

    return bb_surf;
}

GameVideoDesc game_d3d9ex_desc =
{
    .name = "D3D9Ex",
    .init = game_d3d9ex_init,
    .free = game_d3d9ex_free,
    .get_game_device = game_d3d9ex_get_game_device,
    .get_game_texture = game_d3d9ex_get_game_texture,
};
