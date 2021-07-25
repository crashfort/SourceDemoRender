#include "game_shared.h"
#include "game_proc.h"
#include <strsafe.h>
#include <Windows.h>
#include <d3d9.h>
#include <Shlwapi.h>
#include <assert.h>

IDirect3DDevice9Ex* setup_d3d9ex_device;
void* setup_engine_client_ptr;
void* setup_local_player_ptr;

// Backbuffer of the game.
IDirect3DSurface9* setup_content_tex;

// We copy the content of the backbuffer to this texture we create ourselves.
// This texture is created as a shared texture that we can open in d3d11.
HANDLE setup_share_h;
IDirect3DTexture9* setup_share_tex;
IDirect3DSurface9* setup_share_surf;

void(__fastcall* setup_engine_client_exec_cmd_fn)(void* p, void* edx, const char* str);
void(__cdecl* setup_console_msg_fn)(const char* format, ...);
void(__cdecl* setup_console_msg_v_fn)(const char* format, va_list va);
void(__fastcall* setup_view_render_fn)(void* p, void* edx, void* rect);
void(__cdecl* setup_start_movie_fn)(const void* args);
void(__cdecl* setup_end_movie_fn)(const void* args);
int(__cdecl* setup_get_spec_target_fn)();
void*(__cdecl* setup_get_player_by_index_fn)(int index);

GameHook setup_start_movie_hook;
GameHook setup_end_movie_hook;
GameHook setup_view_render_hook;

bool setup_movie_started;

// Can only be enabled once per process.
bool setup_has_enabled_multi_proc;

void client_command(const char* cmd)
{
    setup_engine_client_exec_cmd_fn(setup_engine_client_ptr, NULL, cmd);
}

void game_console_msg(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    game_console_msg_v(format, va);
    va_end(va);
}

void game_console_msg_v(const char* format, va_list va)
{
    setup_console_msg_v_fn(format, va);
}

void __fastcall view_render_override(void* p, void* edx, void* rect)
{
    ((decltype(setup_view_render_fn))setup_view_render_hook.original)(p, edx, rect);

    #if 0
    void* player = **(void***)setup_local_player_ptr;

    int spec = setup_get_spec_target_fn();

    if (spec > 0)
    {
        player = setup_get_player_by_index_fn(spec);
    }

    float vel[3];
    memcpy(vel, (float*)player + 61, sizeof(vel));

    game_console_msg("%5.4f %5.4f %5.4f\n", vel[0], vel[1], vel[2]);
    #endif

    if (setup_movie_started)
    {
        // Copy over the game content to the shared texture.
        // Don't use any filtering type because the source and destinations are both same size.
        setup_d3d9ex_device->StretchRect(setup_content_tex, NULL, setup_share_surf, NULL, D3DTEXF_NONE);

        proc_frame();
    }
}

const char* args_skip_spaces(const char* str)
{
    while (*str && *str == ' ')
    {
        str++;
    }

    return str;
}

const char* args_skip_to_space(const char* str)
{
    while (*str && *str != ' ')
    {
        str++;
    }

    return str;
}

// Allow multiple games to be started at once.
void enable_multi_proc()
{
    // One instance can only remove its own mutex.
    if (setup_has_enabled_multi_proc)
    {
        return;
    }

    HANDLE mutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "hl2_singleton_mutex");

    if (mutex)
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        setup_has_enabled_multi_proc = true;
    }
}

void run_cfg(const char* name)
{
    char path[MAX_PATH];
    path[0] = 0;
    StringCchCatA(path, MAX_PATH, svr_resource_path);
    StringCchCatA(path, MAX_PATH, "\\data\\cfg\\");
    StringCchCatA(path, MAX_PATH, name);

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return;
    }

    LARGE_INTEGER file_size;
    GetFileSizeEx(h, &file_size);

    if (file_size.LowPart == MAXDWORD)
    {
        // Way too big file.
        CloseHandle(h);
        return;
    }

    char* mem = (char*)malloc(file_size.LowPart + 1);
    ReadFile(h, mem, file_size.LowPart, NULL, NULL);
    mem[file_size.LowPart] = 0;

    CloseHandle(h);

    game_console_msg("Running cfg %s\n", name);

    // The file can be executed as is. The game takes care of splitting by newline.
    client_command(mem);
    client_command("\n");

    free(mem);
}

void __cdecl start_movie_override(const void* args)
{
    if (setup_movie_started)
    {
        game_console_msg("Movie already started\n");
        return;
    }

    const char* value_args = (const char*)((u8*)args) + 8;

    // First argument is always startmovie.

    value_args = args_skip_spaces(value_args);
    value_args = args_skip_to_space(value_args);
    value_args = args_skip_spaces(value_args);

    if (*value_args == 0)
    {
        game_console_msg("Usage: startmovie <name> (<profile>)\n");
        game_console_msg("Starts to record a movie with a profile when the game state is reached\n");
        game_console_msg("You need to specify the extension, like .mp4 or .mkv\n");
        game_console_msg("For more information see https://github.com/crashfort/SourceDemoRender\n");
        return;
    }

    value_args = args_skip_spaces(value_args);

    char movie_name[MAX_PATH];
    movie_name[0] = 0;

    char profile[64];
    profile[0] = 0;

    const char* value_args_copy = value_args;
    value_args = args_skip_to_space(value_args);

    s32 movie_name_len = value_args - value_args_copy;
    StringCchCopyNA(movie_name, MAX_PATH, value_args_copy, movie_name_len);

    value_args = args_skip_spaces(value_args);

    // See if a profile was passed in.

    if (*value_args != 0)
    {
        value_args_copy = value_args;
        value_args = args_skip_to_space(value_args);
        s32 profile_len = value_args - value_args_copy;
        StringCchCopyNA(profile, MAX_PATH, value_args_copy, profile_len);
    }

    // Will point to \0 if no extension was provided.
    char* movie_ext = PathFindExtensionA(movie_name);

    const char* h264_allowed_exts[] = {
        ".mp4",
        ".mkv",
        ".mov",
        ".avi",
        NULL
    };

    const char** ext_it = h264_allowed_exts;

    while (*ext_it)
    {
        if (strcmp(*ext_it, movie_ext) == 0)
        {
            break;
        }

        *ext_it++;
    }

    if (*ext_it == NULL)
    {
        game_console_msg("You need to select an extension (container) to use for the movie, like .mp4 or .mkv\n");
        return;
    }

    // The game render target is the first index.
    setup_d3d9ex_device->GetRenderTarget(0, &setup_content_tex);

    // Open the render target here because some games recreate their backbuffer after creation.

    D3DSURFACE_DESC desc;
    setup_content_tex->GetDesc(&desc);

    setup_d3d9ex_device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &setup_share_tex, &setup_share_h);
    setup_share_tex->GetSurfaceLevel(0, &setup_share_surf);

    proc_start(movie_name, profile, setup_share_h);

    // Ensure the game runs at a fixed rate.

    client_command("sv_cheats 1\n");

    char hfr_buf[32];
    StringCchPrintfA(hfr_buf, 32, "host_framerate %d\n", proc_get_game_rate());

    client_command(hfr_buf);

    // Run all user supplied commands.
    run_cfg("svr_movie_start.cfg");

    setup_movie_started = true;

    // First point where we have access to the main thread after the game has started.
    enable_multi_proc();
}

void __cdecl end_movie_override(const void* args)
{
    if (!setup_movie_started)
    {
        game_console_msg("Movie not started\n");
        return;
    }

    proc_end();

    setup_content_tex->Release();
    setup_content_tex = NULL;

    // This handle should not be closed as it is special.
    setup_share_h = NULL;

    setup_share_tex->Release();
    setup_share_tex = NULL;

    setup_share_surf->Release();
    setup_share_surf = NULL;

    // Run all user supplied commands.
    run_cfg("svr_movie_end.cfg");

    setup_movie_started = false;
}

const char* CSS_LIBS[] = {
    "hl2.exe",
    "shaderapidx9.dll",
    "engine.dll",
    "tier0.dll",
    "client.dll",
};

bool setup_wait_for_libs()
{
    const char** libs;
    s32 num_libs;

    switch (game_app_id)
    {
        case SVR_GAME_CSS:
        {
            libs = CSS_LIBS;
            num_libs = SVR_ARRAY_SIZE(CSS_LIBS);
            break;
        }
    }

    if (!game_wait_for_libs(libs, num_libs))
    {
        svr_log("Timeout: Not all libraries were loaded\n");
        return false;
    }

    return true;
}

bool setup_create_hooks()
{
    u8* addr;

    addr = (u8*)game_pattern_scan("A1 ?? ?? ?? ?? 6A 00 56 6A 00 8B 08 6A 15 68 ?? ?? ?? ?? 6A 00 6A 01 6A 01 50 FF 51 5C 85 C0 79 06 C7 06 ?? ?? ?? ??", "shaderapidx9.dll");
    addr += 1;

    setup_d3d9ex_device = **(IDirect3DDevice9Ex***)addr;
    setup_engine_client_ptr = game_create_interface("VEngineClient014", "engine.dll");

    addr = (u8*)game_pattern_scan("A3 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 34 8B C8 E8 ?? ?? ?? ??", "client.dll");
    addr += 1;

    setup_local_player_ptr = addr;

    setup_engine_client_exec_cmd_fn = (decltype(setup_engine_client_exec_cmd_fn))game_get_virtual(setup_engine_client_ptr, 102);
    setup_console_msg_fn = (decltype(setup_console_msg_fn))game_get_export("Msg", "tier0.dll");
    setup_console_msg_v_fn = (decltype(setup_console_msg_v_fn))game_get_export("MsgV", "tier0.dll");
    setup_view_render_fn = (decltype(setup_view_render_fn))game_pattern_scan("55 8B EC 8B 55 08 83 7A 08 00 74 17 83 7A 0C 00 74 11 8B 0D ?? ?? ?? ?? 52 8B 01 FF 50 14 E8 ?? ?? ?? ?? 5D C2 04 00", "client.dll");
    setup_start_movie_fn = (decltype(setup_start_movie_fn))game_pattern_scan("55 8B EC 83 EC 08 83 3D ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ??", "engine.dll");
    setup_end_movie_fn = (decltype(setup_end_movie_fn))game_pattern_scan("80 3D ?? ?? ?? ?? ?? 75 0F 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 04 C3 E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 59 C3", "engine.dll");
    setup_get_spec_target_fn = (decltype(setup_get_spec_target_fn))game_pattern_scan("E8 ?? ?? ?? ?? 85 C0 74 16 8B 10 8B C8 FF 92 ?? ?? ?? ?? 85 C0 74 08 8D 48 08 8B 01 FF 60 24 33 C0 C3", "client.dll");
    setup_get_player_by_index_fn = (decltype(setup_get_player_by_index_fn))game_pattern_scan("55 8B EC 8B 0D ?? ?? ?? ?? 56 FF 75 08 E8 ?? ?? ?? ?? 8B F0 85 F6 74 15 8B 16 8B CE 8B 92 ?? ?? ?? ?? FF D2 84 C0 74 05 8B C6 5E 5D C3 33 C0 5E 5D C3", "client.dll");

    // Remove restriction not allowing to change console variables.
    // This is the "switch to multiplayer or spectators" message.

    addr = (u8*)game_pattern_scan("68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 58 83 3D ?? ?? ?? ?? ??", "engine.dll");
    addr += 1;

    u8 cvar_restrict_patch_bytes[4] = { 0x00, 0x00, 0x00, 0x00 };
    game_apply_patch(addr, cvar_restrict_patch_bytes, 4);

    // Hook the functions so we can override the game flow.

    game_hook_function(setup_start_movie_fn, start_movie_override, &setup_start_movie_hook);
    game_hook_function(setup_end_movie_fn, end_movie_override, &setup_end_movie_hook);
    game_hook_function(setup_view_render_fn, view_render_override, &setup_view_render_hook);

    // Hooking the D3D9Ex Present function to skip presenting is not worthwhile as it does not improve the game frame time.

    return true;
}

bool setup_finalize()
{
    proc_init();

    game_console_msg("---------------\n");
    game_console_msg("SVR initialized\n");
    game_console_msg("---------------\n");

    return true;
}
