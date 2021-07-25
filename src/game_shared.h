#pragma once
#include "svr_common.h"
#include "svr_logging.h"
#include <stdarg.h>

struct GameHook
{
    void* target;
    void* hook;
    void* original;
};

bool game_wait_for_libs(const char** libs, s32 num);
void game_hook_function(void* target, void* hook, GameHook* result_hook);
void* game_pattern_scan(const char* pattern, const char* module);
void game_apply_patch(void* target, u8* bytes, s32 num_bytes);
void* game_create_interface(const char* name, const char* module);
void* game_get_virtual(void* ptr, s32 index);
void* game_get_export(const char* name, const char* module);

// Puts to both game console and log file.
void game_log(const char* format, ...);
void game_log_v(const char* format, va_list va);

// Puts to game console.
void game_console_msg(const char* format, ...);
void game_console_msg_v(const char* format, va_list va);

// Will show a message box and exit the process.
__declspec(noreturn) void game_error(const char* format, ...);

extern const char* svr_resource_path;
extern u32 game_app_id;
