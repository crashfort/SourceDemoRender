#pragma once
#include <stdarg.h>

#ifdef SVR_CONSOLE_DLL
#define SVR_CONSOLE_API __declspec(dllexport)
#else
#define SVR_CONSOLE_API __declspec(dllimport)
#endif

// Shared between svr_standalone.dll and svr_game.dll to print to the game console.

extern "C"
{

SVR_CONSOLE_API void svr_console_init();

// Puts to game console.
SVR_CONSOLE_API void svr_console_msg(const char* format, ...);
SVR_CONSOLE_API void svr_console_msg_v(const char* format, va_list va);

// Puts to game console and log.
SVR_CONSOLE_API void svr_console_msg_and_log(const char* format, ...);
SVR_CONSOLE_API void svr_console_msg_and_log_v(const char* format, va_list va);

}
