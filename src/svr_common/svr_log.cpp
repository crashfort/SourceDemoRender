#include "svr_log.h"  
#include "svr_common.h"
#include <stb_sprintf.h>
#include <assert.h>
#include <Windows.h>

// File logging stuff.

HANDLE log_file_handle;
SRWLOCK log_lock;

void log_function(const char* text, s32 length)
{
    assert(log_file_handle);

    AcquireSRWLockExclusive(&log_lock);
    WriteFile(log_file_handle, text, length, NULL, NULL);
    ReleaseSRWLockExclusive(&log_lock);
}

void svr_init_log(const char* log_file_path, bool append)
{
    // If we are the launcher, we delete the log before creating it to allow changing case.
    // Normally Windows does not allow renaming cases, so we start new.
    if (!append)
    {
        DeleteFileA(log_file_path);
    }

    DWORD open_flags = append ? OPEN_EXISTING : CREATE_ALWAYS;
    log_file_handle = CreateFileA(log_file_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, open_flags, FILE_ATTRIBUTE_NORMAL, NULL);

    // The file might be set to read only or something. Don't bother then.
    if (log_file_handle == INVALID_HANDLE_VALUE)
    {
        log_file_handle = NULL;
        return;
    }

    if (append)
    {
        // We need to move the file pointer to append new text.

        LARGE_INTEGER dist_to_move = {};
        SetFilePointerEx(log_file_handle, dist_to_move, NULL, FILE_END);
    }
}

void svr_shutdown_log()
{
    if (log_file_handle)
    {
        CloseHandle(log_file_handle);
    }
}

// Below log functions not used for integrated SVR, but we may get here still from game_log.

void svr_log(const char* format, ...)
{
    if (log_file_handle == NULL)
    {
        return;
    }

    va_list va;
    va_start(va, format);
    svr_log_v(format, va);
    va_end(va);
}

void svr_log_v(const char* format, va_list va)
{
    if (log_file_handle == NULL)
    {
        return;
    }

    // We don't deal with huge messages and truncate as needed.

    char buf[1024];
    s32 count = SVR_VSNPRINTF(buf, format, va);
    log_function(buf, count);
}
