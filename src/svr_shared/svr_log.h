#pragma once
#include <stdarg.h>

#ifdef SVR_SHARED_DLL
#define SVR_LOG_API __declspec(dllexport)
#else
#define SVR_LOG_API __declspec(dllimport)
#endif

// File logging stuff.
// This is a DLL so the same state can be shared between svr_standalone.dll and svr_game.dll, as they are both loaded in the same process.

extern "C"
{

SVR_LOG_API void svr_init_log(const char* log_file_path, bool append);
SVR_LOG_API void svr_free_log();

SVR_LOG_API void svr_log(const char* format, ...);
SVR_LOG_API void svr_log_v(const char* format, va_list va);

}
