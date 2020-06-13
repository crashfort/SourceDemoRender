#include "game_graphics_d3d9ex.hpp"

#include <svr/log_format.hpp>
#include <svr/os.hpp>

#include <stdint.h>

#include <d3d9.h>

static bool create_standalone_game_texture(IDirect3DDevice9Ex* dev, uint32_t width, uint32_t height, game_texture* out)
{
    auto hr = dev->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &out->texture, (HANDLE*)&out->shared_handle);

    if (FAILED(hr))
    {
        svr::log("d3d9ex: Could not create shared d3d9ex texture (0x{:x})\n", hr);
        return false;
    }

    hr = out->texture->GetSurfaceLevel(0, &out->surface);

    if (FAILED(hr))
    {
        svr::log("d3d9ex: Could not get d3d9ex shared texture surface (0x{:x})\n", hr);
        return false;
    }

    return true;
}

bool game_d3d9ex_create(game_graphics_d3d9ex* ptr, IDirect3DDevice9Ex* device)
{
    // The game render target is the first index.
    IDirect3DSurface9* surface;
    auto hr = device->GetRenderTarget(0, &surface);

    if (FAILED(hr))
    {
        svr::log("Could not get d3d9ex backbuffer render target\n (0x{:x})", hr);
        return false;
    }

    ptr->device = device;
    ptr->game_content = surface;

    D3DSURFACE_DESC desc;
    ptr->game_content->GetDesc(&desc);

    // Create a texture that can be shared with the core system.
    // The game content texture will be copied to this shared texture.
    // The shared texture will be opened in the core system.
    return create_standalone_game_texture(device, desc.Width, desc.Height, &ptr->shared_texture);
}

void game_d3d9ex_release(game_graphics_d3d9ex* ptr)
{
    ptr->game_content->Release();
    ptr->game_content = nullptr;

    // The shared handle is special, it should not be closed by regular means.
    ptr->shared_texture.shared_handle = nullptr;

    ptr->shared_texture.texture->Release();
    ptr->shared_texture.texture = nullptr;

    ptr->shared_texture.surface->Release();
    ptr->shared_texture.surface = nullptr;
}

void game_d3d9ex_copy(game_graphics_d3d9ex* ptr)
{
    // Don't use any filtering type because the source and destinations are both same size.
    ptr->device->StretchRect(ptr->game_content, nullptr, ptr->shared_texture.surface, nullptr, D3DTEXF_NONE);
}
