#pragma once
#include <stdarg.h>

// File logging stuff.
// Used for traceable troubleshooting that is stored in a file that can be shared easily.
// Used in standalone SVR. The launcher creates a log file which is then appended by the game.
// Not used in integrated SVR, as the game can have access writing to the game console the whole time.
// For standalone SVR, there are so many things that can go wrong both in launching and injecting and when running.

void svr_init_log(const char* log_file_path, bool append);

// The launcher must call this before the game process is created.
void svr_shutdown_log();

void svr_log(const char* format, ...);
void svr_log_v(const char* format, va_list va);
