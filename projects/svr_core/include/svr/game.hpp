#pragma once
#include <svr/api.hpp>

namespace svr
{
    struct os_handle;

    // Launches an executable and injects SVR into it.
    SVR_API bool game_launch_inject(const char* exe, const char* game_path, const char* game_id, const char* args, const char* resource_path, os_handle** proc);
}
