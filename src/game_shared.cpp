#include "game_shared.h"
#include <Windows.h>
#include <strsafe.h>
#include "stb_sprintf.h"

using MsgFn = void(__cdecl*)(const char* format, ...);
using MsgVFn = void(__cdecl*)(const char* format, va_list va);

MsgFn console_msg_fn;
MsgVFn console_msg_v_fn;

void game_init()
{
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    console_msg_fn = (MsgFn)GetProcAddress(tier0, "Msg");
    console_msg_v_fn = (MsgVFn)GetProcAddress(tier0, "MsgV");
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
    console_msg_v_fn(format, va);
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

void game_error(const char* format, ...)
{
    char message[1024];

    va_list va;
    va_start(va, format);
    stbsp_vsnprintf(message, 1024, format, va);
    va_end(va);

    svr_log("!!! GAME ERROR: %s\n", message);

    MessageBoxA(NULL, message, "SVR", MB_TASKMODAL | MB_ICONERROR | MB_OK);

    ExitProcess(1);
}
