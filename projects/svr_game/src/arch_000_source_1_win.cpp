#include "game_graphics_d3d9ex.hpp"
#include "game_system.hpp"
#include "game_comp_base.hpp"

#include <svr/reverse.hpp>
#include <svr/log_format.hpp>
#include <svr/graphics.hpp>
#include <svr/tokenize.hpp>
#include <svr/defer.hpp>
#include <svr/swap.hpp>
#include <svr/game_config.hpp>
#include <svr/os.hpp>
#include <svr/mem.hpp>

// Architecture for most Source 1 games on Windows.
// Hooks startmovie, view render and endmovie.
// Uses d3d9ex game graphics and d3d11 graphics backend.

static game_graphics_d3d9ex d3d9ex_graphics;
static game_system* sys;

static bool has_enabled_multi_proc;

// This points to the region of memory that the launcher allocated. It is not freed.
static const char* svr_resource_path;

// Allow multiple games to be started at once.
static void enable_multi_proc()
{
    // One instance can only remove its own mutex.
    if (has_enabled_multi_proc)
    {
        return;
    }

    using namespace svr;

    auto ptr = os_open_mutex("hl2_singleton_mutex");

    if (ptr)
    {
        os_release_mutex(ptr);
        os_close_handle(ptr);
        log("Enabled multiprocess rendering\n");

        has_enabled_multi_proc = true;
    }
}

static bool is_velocity_overlay_allowed(svr::game_config_game* game)
{
    const char* allowed_ids[] = {
        "css-win",
        "mom-win",
    };

    for (auto i : allowed_ids)
    {
        if (strcmp(i, game_config_game_id(game)) == 0)
        {
            return true;
        }
    }

    return false;
}

static bool has_cvar_restrict(svr::game_config_game* game)
{
    const char* non_restricted[] = {
        "csgo-win",
        "gmod-win",
    };

    for (auto i : non_restricted)
    {
        if (strcmp(i, game_config_game_id(game)) == 0)
        {
            return false;
        }
    }

    return true;
}

static void run_cfg(const char* name)
{
    using namespace svr;

    str_builder builder;
    builder.append(svr_resource_path);
    builder.append("data/cfg/");
    builder.append(name);

    mem_buffer buf;

    if (!os_read_file(builder.buf, buf))
    {
        log("Could not open cfg '{}'\n", builder.buf);
        return;
    }

    defer {
        mem_destroy_buffer(buf);
    };

    // New buffer used to add null terminator.
    mem_buffer buf2;

    if (!mem_create_buffer(buf2, buf.size + 1))
    {
        log("Could not create extra cfg buffer\n");
        return;
    }

    defer {
        mem_destroy_buffer(buf2);
    };

    memcpy(buf2.data, buf.data, buf.size);

    auto text = (char*)buf2.data;
    text[buf2.size - 1] = 0;

    log("Running cfg '{}'\n", name);

    // The file can be executed as is. The game takes care of splitting by newline.
    exec_client_command(text);
    exec_client_command("\n");
}

static void process_velocity_overlay()
{
    using namespace svr;

    // In all games that are supported right now that can also support the velocity overlay,
    // the local player is defined as either:
    // 1) static C_BasePlayer *s_pLocalPlayer = NULL;
    // 2) static C_BasePlayer *s_pLocalPlayer[ MAX_SPLITSCREEN_PLAYERS ];
    auto player = **(void***)local_player_ptr;

    auto spec = get_spec_target();

    // TODO
    // This is always 0 for csgo, so this procedure doesn't work for that game.
    // Through cheat engine, the variable which contains the spectate played id has been found, but it's
    // in another location and not in the local player.
    if (spec > 0)
    {
        player = get_player_by_index(spec);
    }

    vec3 vel;
    memcpy(&vel, (float*)player + player_abs_velocity_offset, sizeof(vec3));

    sys_provide_velocity_overlay(sys, vel);
}

static bool open_game_texture()
{
    if (!game_d3d9ex_create(&d3d9ex_graphics, d3d9ex_device_ptr))
    {
        svr::log("Could not create d3d9ex resources\n");
        return false;
    }

    return sys_open_shared_game_texture(sys, d3d9ex_graphics.shared_texture.shared_handle);
}

static void close_game_texture()
{
    game_d3d9ex_release(&d3d9ex_graphics);
}

static void __fastcall view_render_override_000(void* p, void* edx, void* rect)
{
    using namespace svr;

    view_render_hook_000.get_original()(p, edx, rect);

    if (sys_movie_running(sys))
    {
        // Copy over the game contents to the shared texture.
        game_d3d9ex_copy(&d3d9ex_graphics);

        if (sys_use_velocity_overlay(sys))
        {
            process_velocity_overlay();
        }

        // Give it to the system.
        sys_new_frame(sys);
    }
}

static void __cdecl start_movie_override_000(const void* args)
{
    using namespace svr;

    if (sys_movie_running(sys))
    {
        log("Movie already started\n");
        return;
    }

    // This is the full argument list in one string.

    auto v = (const char*)args;
    v += console_cmd_arg_offset;

    auto tokens = tokenize(v);
    auto argc = tokens.size();

    // This command will still get called even with no parameters.

    const char* profile = nullptr;

    if (argc == 1)
    {
        log("Usage: startmovie <name> (<profile>)\n");
        log("\n");
        log("Starts to record a movie with a profile when the game state is reached.\n");
        log("\n");
        log("For more information see https://github.com/crashfort/SourceDemoRender\n");

        return;
    }

    else if (argc == 2)
    {
        log("Using default profile\n");
        profile = nullptr;
    }

    else if (argc == 3)
    {
        const auto& arg = tokens[2];
        profile = arg.c_str();
    }

    const auto& name = tokens[1];

    int width;
    int height;
    materials_get_backbuffer_size(width, height);

    // Open the render target in here because some games recreate their backbuffer after creation.
    if (!open_game_texture())
    {
        log("Could not open game texture\n");
        return;
    }

    if (!sys_start_movie(sys, name.c_str(), profile, width, height))
    {
        log("Could not start movie\n");
        return;
    }

    // Ensure the game runs at a fixed rate.

    log("Setting sv_cheats to 1\n");
    log("Setting host_framerate to {}\n", sys_get_game_rate(sys));

    cvar_set_value("sv_cheats", 1);
    cvar_set_value("host_framerate", (int)sys_get_game_rate(sys));

    // Run all user supplied commands.
    run_cfg("svr_movie_start.cfg");

    // This is the first location where we have access to the main thread while the game is running.
    // Here we have access to the launcher mutex. Disable it so we can launch more games for rendering.
    enable_multi_proc();
}

static void __cdecl end_movie_override_000(const void* args)
{
    using namespace svr;

    if (!sys_movie_running(sys))
    {
        log("Movie not started\n");
        return;
    }

    sys_end_movie(sys);
    close_game_texture();

    // Run all user supplied commands.
    run_cfg("svr_movie_end.cfg");
}

bool arch_code_000_source_1_win(svr::game_config_game* game, const char* resource_path)
{
    using namespace svr;

    auto graphics = graphics_create_d3d11_backend(resource_path);

    if (graphics == nullptr)
    {
        log("Could not create graphics backend\n");
        return false;
    }

    log("Using d3d11 graphics backend\n");

    auto temp_sys = sys_create(resource_path, graphics);

    defer {
        if (temp_sys) sys_destroy(temp_sys);
    };

    auto can_have_veloc_overlay = is_velocity_overlay_allowed(game);

    // Resolve the required components for this arch.

    if (!find_resolve_component(game, "d3d9ex-device")) return false;
    if (!find_resolve_component(game, "materials-ptr")) return false;
    if (!find_resolve_component(game, "materials-get-bbuf-size")) return false;
    if (!find_resolve_component(game, "engine-client-ptr")) return false;
    if (!find_resolve_component(game, "engine-client-exec-cmd")) return false;
    if (!find_resolve_component(game, "console-print-message")) return false;
    if (!find_resolve_component(game, "view-render")) return false;
    if (!find_resolve_component(game, "start-movie-cmd")) return false;
    if (!find_resolve_component(game, "end-movie-cmd")) return false;
    if (!find_resolve_component(game, "console-cmd-args-offset")) return false;

    // This is only needed for games that do not allow changing fps_max when playing.
    if (has_cvar_restrict(game))
    {
        if (!find_resolve_component(game, "cvar-remove-restrict")) return false;
    }

    // Skip these on some games.
    if (can_have_veloc_overlay)
    {
        log("Velocity overlay supported\n");

        if (!find_resolve_component(game, "local-player-ptr")) return false;
        if (!find_resolve_component(game, "get-spec-target")) return false;
        if (!find_resolve_component(game, "get-player-by-index")) return false;
        if (!find_resolve_component(game, "player-abs-velocity-offset")) return false;
    }

    reverse_init();

    if (view_render_addr_000) reverse_hook_function(view_render_addr_000, view_render_override_000, &view_render_hook_000);
    if (start_movie_addr_000) reverse_hook_function(start_movie_addr_000, start_movie_override_000, &start_movie_hook_000);
    if (end_movie_addr_000) reverse_hook_function(end_movie_addr_000, end_movie_override_000, &end_movie_hook_000);

    if (!reverse_enable_all_hooks())
    {
        log("Could not enable hooks\n");
        return false;
    }

    log("Redirecting output to game console\n");

    log_set_function([](void* context, const char* text)
    {
        console_message(text);
    }, nullptr);

    swap_ptr(sys, temp_sys);

    sys_set_velocity_overlay_support(sys, can_have_veloc_overlay);
    svr_resource_path = resource_path;
    return true;
}
