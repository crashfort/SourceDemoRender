#pragma once
#include <svr/reverse.hpp>

#include <stdint.h>

struct IDirect3DDevice9Ex;

namespace svr
{
    struct game_config_game;
}

bool find_resolve_component(svr::game_config_game* game, const char* name);

// This is every possible component that is possible to be resolvable from the game config.

extern IDirect3DDevice9Ex* d3d9ex_device_ptr;
extern void* materials_ptr;
extern void(__fastcall* materials_get_bbuf_size_addr_000)(void* p, void* edx, int& width, int& height);
extern void* engine_client_ptr;
extern void(__fastcall* client_exec_cmd_addr_000)(void* p, void* edx, const char* str);
extern void(__cdecl* console_msg_addr_000)(const char* format, ...);
extern void(__fastcall* view_render_addr_000)(void* p, void* edx, void* rect);
extern void(__cdecl* start_movie_addr_000)(const void* args);
extern void(__cdecl* end_movie_addr_000)(const void* args);
extern ptrdiff_t console_cmd_arg_offset;
extern void* local_player_ptr;
extern int(__cdecl* get_spec_target_000)();
extern void*(__cdecl* get_player_by_index_000)(int value);
extern ptrdiff_t player_abs_velocity_offset;

extern svr::reverse_hook_template<decltype(view_render_addr_000)> view_render_hook_000;
extern svr::reverse_hook_template<decltype(start_movie_addr_000)> start_movie_hook_000;
extern svr::reverse_hook_template<decltype(end_movie_addr_000)> end_movie_hook_000;

void materials_get_backbuffer_size(int& width, int& height);
void exec_client_command(const char* value);
void cvar_set_value(const char* name, int value);
void console_message(const char* value);
int get_spec_target();
void* get_player_by_index(int value);
