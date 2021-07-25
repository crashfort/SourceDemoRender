#pragma once
#include <stdarg.h>

void svr_game_init_log(const char* resource_path);
void svr_launcher_init_log();
void svr_launcher_shutdown_log();

void svr_log(const char* format, ...);
void svr_log_v(const char* format, va_list va);
