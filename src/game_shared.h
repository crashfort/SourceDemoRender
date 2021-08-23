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

// Will show a message box and exit the process.
// Does not work well when the game is in fullscreen, because the message box doesn't get focus because Windows is epic.
__declspec(noreturn) void game_error(const char* format, ...);
