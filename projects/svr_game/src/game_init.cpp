#include <svr/launcher.hpp>
#include <svr/os.hpp>
#include <svr/log_format.hpp>
#include <svr/defer.hpp>
#include <svr/game_config.hpp>
#include <svr/str.hpp>
#include <svr/config.hpp>

#include "game_external.hpp"

#include <chrono>
#include <thread>

static bool wait_for_game_libs(const char** libs, size_t size)
{
    using namespace std::chrono_literals;
    using namespace svr;

    // Alternate method instead of hooking the LoadLibrary family of functions.
    // We don't need particular accuracy so this is good enough and much simpler.

    log("Waiting for libraries:\n");

    for (size_t i = 0; i < size; i++)
    {
        log("  - {}\n", libs[i]);
    }

    for (size_t i = 0; i < 100; i++)
    {
        if (os_check_proc_modules(os_get_proc_handle_self(), libs, size) == size)
        {
            return true;
        }

        std::this_thread::sleep_for(2s);
    }

    log("Gave up after 200 seconds, aborting\n");
    return false;
}

namespace svr
{
    // This is the main entry point within the game process.
    // This is before the game has actually started at all, and before most libraries
    // are loaded too.
    // The current directory of the process right now is in the SVR resource directory.
    extern "C" SVR_EXPORT void svr_init(const launcher_init_data& data)
    {
        auto t = std::thread([=]()
        {
            // Open up the communication pipe to the launcher.
            auto pipe = launcher_open_com_pipe();

            defer {
                // Tell the launcher that we will no longer be sending any messages.
                os_close_handle(pipe);
            };

            // Make subsequent logs write to the launcher pipe.
            log_set_function([](void* context, const char* text)
            {
                auto pipe = (os_handle*)context;
                auto len = strlen(text);

                os_write_pipe(pipe, text, len);
            }, pipe);

            // Tell the launcher that we have opened the pipe.
            launcher_signal_com_link();

            log("Using game thread {}\n", os_get_thread_id_self());

            str_builder builder;
            builder.append(data.resource_path);
            builder.append("data/game-config.json");

            auto config = config_open_json(builder.buf);

            defer {
                config_destroy(config);
            };

            auto cfg = game_config_parse(config_root(config));

            defer {
                game_config_destroy(cfg);
            };

            // It's confirmed at this point that the game does exist in the game config.
            // The launcher does the initial sweeps.
            auto game = game_config_find_game(cfg, data.game_id);

            if (!wait_for_game_libs(game_config_game_libs(game), game_config_game_libs_size(game)))
            {
                log("Could not wait for required libraries\n");

                launcher_signal_fail_event();
                return;
            }

            log("Libraries loaded, initializing\n");

            if (!game_external_init(game, data.resource_path))
            {
                log("Could not initialize game. Terminating\n");
                launcher_signal_fail_event();

                os_terminate_proc_self();
            }

            launcher_signal_success_event();
        });

        t.detach();
    }
}
