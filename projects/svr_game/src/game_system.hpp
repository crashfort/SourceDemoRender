#pragma once
#include <stdint.h>

namespace svr
{
    struct os_handle;
    struct graphics_backend;
}

struct game_system;

// Initializes the system.
// Takes ownership of the graphics backend.
game_system* sys_create(const char* resource_path, svr::graphics_backend* graphics);

void sys_destroy(game_system* sys);

uint32_t sys_get_game_rate(game_system* sys);

// Opens up a shared texture and uses it as the game content.
// This is an alternative function.
bool sys_open_shared_game_texture(game_system* sys, svr::os_handle* ptr);

// Tells the system that there is a new video frame available.
void sys_new_frame(game_system* sys);

// Returns whether or not a movie is currently running.
bool sys_movie_running(game_system* sys);

void sys_set_velocity_overlay_support(game_system* sys, bool value);
bool sys_use_velocity_overlay(game_system* sys);
void sys_provide_velocity_overlay(game_system* sys, float x, float y, float z);

// Starts a movie.
bool sys_start_movie(game_system* sys, const char* name, const char* profile, uint32_t width, uint32_t height);

// Ends a movie.
void sys_end_movie(game_system* sys);
