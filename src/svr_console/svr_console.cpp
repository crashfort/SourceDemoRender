#include "svr_common.h"
#include "svr_console.h"
#include "svr_log.h"
#include <Windows.h>

using GameMsgFn = void(__cdecl*)(const char* format, ...);

GameMsgFn svr_console_msg_fn;

void svr_console_init()
{
    HMODULE module = GetModuleHandleA("tier0.dll");

    if (module == NULL)
    {
        return;
    }

    svr_console_msg_fn = (GameMsgFn)GetProcAddress(module, "Msg");
}

void svr_console_msg(const char* format, ...)
{
    if (svr_console_msg_fn == NULL)
    {
        return;
    }

    va_list va;
    va_start(va, format);
    svr_console_msg_v(format, va);
    va_end(va);
}

void svr_console_msg_v(const char* format, va_list va)
{
    if (svr_console_msg_fn == NULL)
    {
        return;
    }

    char buf[1024];
    SVR_VSNPRINTF(buf, format, va);

    svr_console_msg_fn(buf);
}

void svr_console_msg_and_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    svr_console_msg_and_log_v(format, va);
    va_end(va);
}

void svr_console_msg_and_log_v(const char* format, va_list va)
{
    svr_log_v(format, va);
    svr_console_msg_v(format, va);
}
