#pragma once
#include "svr_common.h"
#include "svr_logging.h"
#include <stdarg.h>

void game_init();

// Puts to both game console and log file.
void game_log(const char* format, ...);
void game_log_v(const char* format, va_list va);

// Puts to game console.
void game_console_msg(const char* format, ...);
void game_console_msg_v(const char* format, va_list va);
