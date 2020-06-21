#include <svr/game.hpp>
#include <svr/launcher.hpp>
#include <svr/os.hpp>
#include <svr/log_format.hpp>
#include <svr/game_config.hpp>
#include <svr/defer.hpp>
#include <svr/str.hpp>
#include <svr/config.hpp>

static bool does_game_exist(const char* resource_path, const char* game_id)
{
    using namespace svr;

    str_builder builder;
    builder.append(resource_path);
    builder.append("data/game-config.json");

    auto config = config_open_json(builder.buf);

    if (config == nullptr)
    {
        log("Could not open game config using path '{}'\n", builder.buf);
        return false;
    }

    defer {
        config_destroy(config);
    };

    auto cfg = game_config_parse(config_root(config));

    if (cfg == nullptr)
    {
        log("Could not parse game config using path '{}'\n", builder.buf);
        return false;
    }

    defer {
        game_config_destroy(cfg);
    };

    return game_config_find_game(cfg, game_id) != nullptr;
}

namespace svr
{
    bool game_launch_inject(const char* exe, const char* game_path, const char* game_id, const char* args, const char* resource_path, os_handle** proc)
    {
        log("This is a standalone version of SVR. Interoperability with other applications may not work\n");

        bool verify_installation(const char* resource_path);

        if (!verify_installation(resource_path))
        {
            log("Installation is incomplete\n");
            return false;
        }

        if (!does_game_exist(resource_path, game_id))
        {
            log("Game '{}' not found in game config\n", game_id);
            return false;
        }

        // Require that these args are included no matter what.
        str_builder args_builder;
        args_builder.append("-steam -insecure +sv_lan 1 -console ");
        args_builder.append(args);

        log("Trying to launch and inject game:\n");
        log("Executable: '{}'\n", exe);
        log("Game path: '{}'\n", game_path);
        log("Game id: '{}'\n", game_id);
        log("Parameters: '{}'\n", args_builder.buf);
        log("Resource path: '{}'\n", resource_path);

        log("Starting process\n");

        // This is the pipe the game process will write log messages to.
        // We will display the output alongside our own.
        auto pipe = launcher_create_com_pipe();

        if (pipe == nullptr)
        {
            log("Could not create communication pipe server\n");
            return false;
        }

        defer {
            os_close_handle(pipe);
        };

        // The game process will set this event once it has connected to the pipe.
        auto com_link_event = launcher_create_com_link_event();

        if (com_link_event == nullptr)
        {
            log("Could not create communication link event\n");
            return false;
        }

        defer {
            os_close_handle(com_link_event);
        };

        os_start_proc_desc desc = {};
        desc.suspended = true;

        os_handle* proc_handle;
        os_handle* thread_handle;

        if (!os_start_proc(exe, game_path, args_builder.buf, &desc, &proc_handle, &thread_handle))
        {
            log("Could not start game process\n");
            return false;
        }

        defer {
            if (proc_handle) os_close_handle(proc_handle);
            os_close_handle(thread_handle);
        };

        log("Created process with id {} and thread id {}\n", os_get_proc_id(proc_handle), os_get_thread_id(thread_handle));

        // The game process will open and set this when it is done with everything.
        auto success_event = launcher_create_success_event();
        auto fail_event = launcher_create_fail_event();

        if (success_event == nullptr)
        {
            log("Could not create success notification event\n");
            return false;
        }

        if (fail_event == nullptr)
        {
            log("Could not create failure notification event\n");
            return false;
        }

        defer {
            os_close_handle(success_event);
            os_close_handle(fail_event);
        };

        bool inject_process(svr::os_handle* proc, svr::os_handle* thread, const char* resource_path, const char* game_id);

        if (!inject_process(proc_handle, thread_handle, resource_path, game_id))
        {
            log("Could not inject into game\n");
            return false;
        }

        log("Resuming process\n");

        // Let the process actually start now.
        os_resume_thread(thread_handle);

        os_handle* waitables[] = {
            proc_handle,
            success_event,
            fail_event,
        };

        auto waited = os_handle_wait_any(waitables, 3, -1);

        // Require that the game has opened the pipe.
        if (os_handle_wait(com_link_event, -1))
        {
            // Read back what has been written to us.

            char buf[1024];
            size_t read;

            while (os_read_pipe(pipe, buf, sizeof(buf) - 1, &read))
            {
                buf[read] = 0;
                log(buf);
            }
        }

        if (waited == proc_handle)
        {
            log("Process exited before initialization finished\n");
            return false;
        }

        else if (waited == fail_event)
        {
            log("Initialization failure\n");
            return false;
        }

        if (proc)
        {
            *proc = proc_handle;
            proc_handle = nullptr;
        }

        log("Done\n");
        return true;
    }
}
