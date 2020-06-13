#pragma once
#include <svr/api.hpp>

namespace svr
{
    struct os_handle;

    struct launcher_init_data
    {
        // Absolute path to the SVR bin directory.
        // Will always end in a slash.
        const char* resource_path;

        // The name of the game id inside the game config to work against.
        const char* game_id;
    };

    // The type of the exported svr_init function in svr_game.
    using svr_init_type = void(__cdecl*)(const launcher_init_data& data);

    // Creates a process shared synchronization event.
    // This is intended to be run in the launcher.
    SVR_API os_handle* launcher_create_completion_event();

    // Signals the process shared completion event that the initialization within
    // the game has completed.
    // This is intended to be run in the game process.
    SVR_API void launcher_signal_completion_event();

    // Creates a pipe endpoint that is to receive log messages from the game.
    // This is intended to be run in the launcher.
    SVR_API os_handle* launcher_create_com_pipe();

    // Creates a pipe endpoint that is to write log messages to the launcher.
    // This is intended to be run in the game process.
    SVR_API os_handle* launcher_open_com_pipe();

    // Creates a process shared synchronization event.
    // Used to wait for the game process to open the communication pipe.
    // This is intended to be run in the launcher.
    SVR_API os_handle* launcher_create_com_link_event();

    // Signals the process shared event that the game process has opened the communication pipe.
    // This is intended to be run in the game process.
    SVR_API void launcher_signal_com_link();
}
