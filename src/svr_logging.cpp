#include "svr_logging.h"
#include "svr_common.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#include <assert.h>
#include <Windows.h>
#include <strsafe.h>

// File logging stuff.

HANDLE log_file_handle;

void log_function(const char* text, s32 length)
{
    if (log_file_handle)
    {
        WriteFile(log_file_handle, text, length, NULL, NULL);
    }
}

void svr_game_init_log(const char* resource_path)
{
    char full_log_path[MAX_PATH];
    full_log_path[0] = 0;
    StringCchCatA(full_log_path, MAX_PATH, resource_path);
    StringCchCatA(full_log_path, MAX_PATH, "\\data\\SVR_LOG.TXT");

    // We are the game, we want to append to the log file that the launcher has created.
    log_file_handle = CreateFileA(full_log_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // The file might be set to read only or something. Don't bother then.
    if (log_file_handle == INVALID_HANDLE_VALUE)
    {
        log_file_handle = NULL;
        return;
    }

    // We need to move the file pointer to append new text to the end.

    LARGE_INTEGER dist_to_move = {};
    SetFilePointerEx(log_file_handle, dist_to_move, NULL, FILE_END);
}

void svr_launcher_init_log()
{
    // We are the launcher, we will create the log file and truncate it to start fresh.
    log_file_handle = CreateFileA("data\\SVR_LOG.TXT", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // The file might be set to read only or something. Don't bother then.
    if (log_file_handle == INVALID_HANDLE_VALUE)
    {
        log_file_handle = NULL;
    }
}

void svr_launcher_shutdown_log()
{
    if (log_file_handle)
    {
        CloseHandle(log_file_handle);
    }
}

void svr_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    svr_log_v(format, va);
    va_end(va);
}

void svr_log_v(const char* format, va_list va)
{
    if (log_file_handle == NULL) return;

    // We don't deal with huge messages.
    // The message is truncated accordingly to the buffer size.

    char buf[1024];
    s32 count = stbsp_vsnprintf(buf, 1023, format, va);
    log_function(buf, count);
}
