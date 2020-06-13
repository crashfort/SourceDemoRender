#pragma once

struct IDirect3DDevice9Ex;
struct IDirect3DSurface9;
struct IDirect3DTexture9;

namespace svr
{
    struct os_handle;
}

struct game_texture
{
    svr::os_handle* shared_handle;
    IDirect3DTexture9* texture;
    IDirect3DSurface9* surface;
};

struct game_graphics_d3d9ex
{
    IDirect3DDevice9Ex* device;
    IDirect3DSurface9* game_content;

    game_texture shared_texture;
};

bool game_d3d9ex_create(game_graphics_d3d9ex* ptr, IDirect3DDevice9Ex* device, IDirect3DSurface9* surface);
void game_d3d9ex_release(game_graphics_d3d9ex* ptr);
void game_d3d9ex_copy(game_graphics_d3d9ex* ptr);
