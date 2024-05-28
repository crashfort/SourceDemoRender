#include "svr_common.h"
#include "svr_alloc.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <Windows.h>
#include <assert.h>
#include <mfapi.h>
#include <strsafe.h>

// Prefer to use this instead of calling Release yourself since you can use this to see the actual reference count.
void svr_release(struct IUnknown* p)
{
    assert(p);
    auto n = p->Release();
    s32 a = 5;
}

void svr_maybe_close_handle(void** h)
{
    if (*h)
    {
        CloseHandle(*h);
        *h = NULL;
    }
}

void svr_maybe_free(void** addr)
{
    if (*addr)
    {
        svr_free(*addr);
        *addr = NULL;
    }
}

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
    SVR_VSNPRINTF(svr_va_buf, format, va);
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
    s32 str_len = strlen(str);
    s32 suffix_len = strlen(suffix);

    if (suffix_len > str_len)
    {
        return false;
    }

    str += str_len - suffix_len;

    return !strcmp(str, suffix);
}

s32 svr_to_utf16(const char* value, s32 value_length, wchar* buf, s32 buf_chars)
{
    s32 length = MultiByteToWideChar(CP_UTF8, 0, value, value_length, buf, buf_chars);

    if (length < buf_chars)
    {
        buf[length] = 0;
    }

    return length;
}

template <class T>
bool svr_are_values_sorted_priv(T* values, s32 num)
{
    if (num < 2)
    {
        return true;
    }

    T prev = values[0];

    for (s32 i = 1; i < num; i++)
    {
        if (prev > values[i])
        {
            return false;
        }

        prev = values[i];
    }

    return true;
}

bool svr_is_sorted(s32* idxs, s32 num)
{
    return svr_are_values_sorted_priv(idxs, num);
}

bool svr_are_idxs_unique(s32* idxs, s32 num)
{
    assert(svr_is_sorted(idxs, num));

    if (num < 2)
    {
        return true;
    }

    for (s32 i = 1; i < num; i++)
    {
        s32 prev = idxs[i - 1];
        s32 cur = idxs[i];

        if (prev == cur)
        {
            return false;
        }
    }

    return true;
}

void svr_check_all_mask(bool* mask, s32 num, bool* all_false, bool* all_true)
{
    bool are_all_false = true;
    bool are_all_true = true;

    for (s32 i = 0; (i < num) && (are_all_false || are_all_true); i++)
    {
        are_all_false &= !mask[i];
        are_all_true &= mask[i];
    }

    *all_false = are_all_false;
    *all_true = are_all_true;
}

char* svr_read_file_as_string(const char* path, SvrReadFileFlags flags)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    char* ret = NULL;

    LARGE_INTEGER large;
    GetFileSizeEx(h, &large);

    s32 ceiling = 0;

    if (flags & SVR_READ_FILE_FLAGS_NEW_LINE)
    {
        ceiling++;
    }

    if (large.HighPart == 0 && large.LowPart < (INT32_MAX - ceiling))
    {
        ret = (char*)svr_alloc(large.LowPart + 1 + ceiling);

        ReadFile(h, ret, large.LowPart, NULL, NULL);

        if (flags & SVR_READ_FILE_FLAGS_NEW_LINE)
        {
            ret[large.LowPart - 1] = '\n';
        }

        ret[large.LowPart] = 0;
    }

    CloseHandle(h);

    return ret;
}

const char* svr_read_line(const char* start, char* dest, s32 dest_size)
{
    dest[0] = 0;

    const char* ptr = start;

    for (; *ptr != 0;)
    {
        s32 nl = svr_is_newline(ptr);

        if (nl != 0)
        {
            const char* end = ptr;
            s32 line_length = end - start;
            s32 use_length = svr_min(line_length, dest_size - 1);

            memcpy(dest, start, use_length);
            dest[use_length] = 0;

            ptr += nl;
            break;
        }

        else
        {
            ptr++;
        }
    }

    return ptr;
}

s32 svr_is_newline(const char* seq)
{
    if (seq[0] == 0)
    {
        return 0;
    }

    if (seq[0] == '\n')
    {
        return 1;
    }

    if (seq[0] == '\r' && seq[1] != '\n')
    {
        return 0;
    }

    if (seq[0] == '\r' && seq[1] == '\n')
    {
        return 2;
    }

    return 0;
}

bool svr_is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

const char* svr_advance_until_after_whitespace(const char* text)
{
    const char* ptr = text;

    while (*ptr != 0 && svr_is_whitespace(*ptr))
    {
        ptr++;
    }

    return ptr;
}

const char* svr_advance_until_whitespace(const char* text)
{
    const char* ptr = text;

    while (*ptr != 0 && !svr_is_whitespace(*ptr))
    {
        ptr++;
    }

    return ptr;
}

const char* svr_advance_until_char(const char* text, char c)
{
    const char* ptr = text;

    while (*ptr != 0 && *ptr != c)
    {
        ptr++;
    }

    return ptr;
}

const char* svr_advance_quote(const char* text)
{
    const char* ptr = text;

    if (*ptr != 0 && *ptr == '\"')
    {
        ptr++;
    }

    return ptr;
}

const char* svr_advance_string(bool quoted, const char* text)
{
    const char* ptr = text;

    if (quoted)
    {
        // If we are quoted, only break once we reach the ending quote.
        while (*ptr != 0 && *ptr != '\"')
        {
            ptr++;
        }
    }

    else
    {
        // If we are not quoted, break on first whitespace.
        while (*ptr != 0 && !svr_is_whitespace(*ptr))
        {
            ptr++;
        }
    }

    return ptr;
}

// Extract a token at the given position in text.
// For unquoted strings, this will break on the first whitespace.
// For quoted strings, this will break on quote end.
const char* svr_extract_string(const char* text, char* dest, s32 dest_size)
{
    const char* ptr = text;
    dest[0] = 0;

    bool quoted = *ptr == '\"';
    ptr = svr_advance_quote(ptr); // Maybe go inside quote.
    const char* next_ptr = svr_advance_string(quoted, ptr); // Read content.
    s32 dist = next_ptr - ptr; // Content length.
    StringCchCopyNA(dest, dest_size, ptr, dist);
    next_ptr = svr_advance_quote(next_ptr); // Maybe go outside quote.

    return next_ptr;
}

// For backslashes only, makes \\ into \ (or \\\\ into \\).
void svr_unescape_path(const char* buf, char* dest, s32 dest_size)
{
    const char* ptr = buf;
    s32 i = 0;

    for (; *ptr != 0 && i < dest_size; ptr++)
    {
        if (*ptr == '\\' && *(ptr + 1) == '\\')
        {
            continue;
        }

        dest[i] = *ptr;
        i++;
    }

    dest[i] = 0;
}

bool svr_idx_in_range(s32 idx, s32 size)
{
    return idx >= 0 && idx < size;
}

SvrSplitTime svr_split_time(s64 us)
{
    s64 micros = us % 1000000;

    SvrSplitTime ret;
    ret.seconds = (us / 1000000) % 60;
    ret.minutes = (us / 1000000 / 60) % 60;
    ret.hours = (us / 1000000 / 60 / 60);
    ret.millis = svr_rescale(micros, 1000, 1000000);

    return ret;
}

s64 svr_rescale(s64 a, s64 b, s64 c)
{
    return MFllMulDiv(a, b, c, c / 2);
}

bool svr_check_all_true(bool* opts, s32 num)
{
    for (s32 i = 0; i < num; i++)
    {
        if (!opts[i])
        {
            return false;
        }
    }

    return true;
}

bool svr_check_one_true(bool* opts, s32 num)
{
    for (s32 i = 0; i < num; i++)
    {
        if (opts[i])
        {
            return true;
        }
    }

    return false;
}

s32 svr_count_num_true(bool* opts, s32 num)
{
    s32 ret = 0;

    for (s32 i = 0; i < num; i++)
    {
        ret += opts[i];
    }

    return ret;
}

bool svr_does_file_exist(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA attr;
    auto res = GetFileAttributesExA(path, GetFileExInfoStandard, &attr);

    return res != 0;
}

void svr_trim_right(char* buf, s32 length)
{
    s32 len = length;
    char* start = buf;
    char* end = buf + len - 1;

    while (end != start && svr_is_whitespace(*end))
    {
        end--;
    }

    end++;
    *end = 0;
}
