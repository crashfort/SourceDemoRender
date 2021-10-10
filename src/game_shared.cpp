#include "game_shared.h"
#include <Windows.h>
#include "stb_sprintf.h"
#include <assert.h>

// CSGO doesn't have MsgV that takes a va_list. Don't make a special case for CSGO and just format ourselves.

using MsgFn = void(__cdecl*)(const char* format, ...);

MsgFn console_msg_fn;

void game_init()
{
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    console_msg_fn = (MsgFn)GetProcAddress(tier0, "Msg");

    assert(console_msg_fn);
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
    stbsp_vsnprintf(buf, 1024, format, va);

    console_msg_fn(buf);
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
