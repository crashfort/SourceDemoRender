#include "game_console.h"
#include "svr_log.h"
#include <Windows.h>

// CSGO doesn't have MsgV that takes a va_list. Don't make a special case for CSGO and just format ourselves.

using GmMsgFn = void(__cdecl*)(const char* format, ...);

GmMsgFn gm_console_msg_fn;

void game_console_init()
{
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    gm_console_msg_fn = (GmMsgFn)GetProcAddress(tier0, "Msg");
}

void game_console_msg(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    game_console_msg_v(format, va);
    va_end(va);
}

void game_console_msg_v(const char* format, va_list va)
{
    char buf[1024];
    SVR_VSNPRINTF(buf, format, va);

    gm_console_msg_fn(buf);
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
