#include "game_console.h"
#include "svr_log.h"
#include <Windows.h>

using GameMsgFn = void(__cdecl*)(const char* format, ...);

GameMsgFn game_console_msg_fn;

void game_console_init()
{
    HMODULE module = GetModuleHandleA("tier0.dll");

    if (module == NULL)
    {
        svr_log("WARNING: Could not find tier0.dll for console messages\n");
        return;
    }

    game_console_msg_fn = (GameMsgFn)GetProcAddress(module, "Msg");
}

void game_console_msg(const char* format, ...)
{
    if (game_console_msg_fn == NULL)
    {
        return;
    }

    va_list va;
    va_start(va, format);
    game_console_msg_v(format, va);
    va_end(va);
}

void game_console_msg_v(const char* format, va_list va)
{
    if (game_console_msg_fn == NULL)
    {
        return;
    }

    char buf[1024];
    SVR_VSNPRINTF(buf, format, va);

    game_console_msg_fn(buf);
}

void game_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    game_log_v(format, va);
    va_end(va);
}

void game_log_v(const char* format, va_list va)
{
    svr_log_v(format, va);
    game_console_msg_v(format, va);
}
