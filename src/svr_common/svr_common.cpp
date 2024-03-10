#include "svr_common.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

s32 svr_copy_string(const char* source, char* dest, s32 dest_chars)
{
    s32 len = svr_min(dest_chars - 1, (s32)strlen(source));
    memcpy(dest, source, len);
    dest[len] = 0;

    return len;
}

thread_local char svr_va_buf[4096];

const char* svr_va(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    vsnprintf(svr_va_buf, SVR_ARRAY_SIZE(svr_va_buf), format, va);
    va_end(va);

    return svr_va_buf;
}

bool svr_starts_with(const char* str, const char* prefix)
{
    while (*prefix != 0)
    {
        if (*prefix != *str)
        {
            return false;
        }

        prefix++;
        str++;
    }

    return true;
}

bool svr_ends_with(const char* str, const char* suffix)
{
    auto str_len = strlen(str);
    auto suffix_len = strlen(suffix);

    if (suffix_len > str_len)
    {
        return false;
    }

    str += str_len - suffix_len;

    return !strcmp(str, suffix);
}
