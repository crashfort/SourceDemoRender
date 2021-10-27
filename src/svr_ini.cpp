#include "svr_ini.h"
#include <Windows.h>
#include <strsafe.h>
#include <assert.h>

s32 ini_is_newline(const char* seq)
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

bool ini_is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

void ini_parse_line(char* line_buf, SvrIniLine* ini_line, SvrIniTokenType* type)
{
    const s32 MAX_INI_TOKENS = 3;

    char* ptr = line_buf;

    char* tokens[MAX_INI_TOKENS] = { NULL, NULL, NULL };
    s32 token_index = 0;

    tokens[token_index] = ptr;
    token_index++;

    for (; *ptr != 0; ptr++)
    {
        // Allowed to use this character on the value side.
        if (token_index == 1 && *ptr == '#')
        {
            *type = SVR_INI_OTHER;
            return;
        }

        else if (*ptr == '=')
        {
            assert(token_index < MAX_INI_TOKENS);

            tokens[token_index] = ptr + 1;
            token_index++;
        }
    }

    assert(token_index < MAX_INI_TOKENS);

    tokens[token_index] = ptr;
    token_index++;

    *type = SVR_INI_KV;

    ini_line->title[0] = 0;
    ini_line->value[0] = 0;

    s32 title_length = (tokens[1] - tokens[0]) - 1;
    s32 value_length = (tokens[2] - tokens[1]);

    if (title_length == 0)
    {
        return;
    }

    StringCchCopyNA(ini_line->title, SVR_INI_TOKEN_BUF_SIZE, tokens[0], title_length);
    StringCchCopyNA(ini_line->value, SVR_INI_TOKEN_BUF_SIZE, tokens[1], value_length);
}

SvrIniLine svr_alloc_ini_line()
{
    SvrIniLine ret;
    ret.title = (char*)malloc(SVR_INI_TOKEN_BUF_SIZE);
    ret.value = (char*)malloc(SVR_INI_TOKEN_BUF_SIZE);

    return ret;
}

void svr_free_ini_line(SvrIniLine* line)
{
    free(line->title);
    free(line->value);
}

bool svr_open_ini_read(const char* path, SvrIniMem* mem)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool ret = false;

    LARGE_INTEGER large;
    GetFileSizeEx(h, &large);

    if (large.HighPart == 0 && large.LowPart < MAXDWORD)
    {
        mem->mem = malloc(large.LowPart + 1);

        DWORD nr;
        ReadFile(h, mem->mem, large.LowPart, &nr, NULL);

        mem->mov_str = (char*)mem->mem;
        mem->mov_str[large.LowPart] = 0;
        mem->line_buf = (char*)malloc(SVR_INI_LINE_BUF_SIZE);

        ret = true;
    }

    CloseHandle(h);
    return ret;
}

bool svr_read_ini_line(SvrIniMem* mem)
{
    if (*mem->mov_str == 0)
    {
        return false;
    }

    char* line_start = mem->mov_str;

    // Skip all blank lines.

    for (; *mem->mov_str != 0;)
    {
        if (s32 nl = ini_is_newline(mem->mov_str))
        {
            char* line_end = mem->mov_str;
            s32 line_length = line_end - line_start;

            if (line_length > 0)
            {
                StringCchCopyNA(mem->line_buf, SVR_INI_LINE_BUF_SIZE, line_start, line_length); 
            }

            mem->mov_str += nl;
            line_start = mem->mov_str;

            if (line_length > 0)
            {
                return true;
            }
        }

        else
        {
            mem->mov_str++;
        }
    }

    if (line_start == mem->mov_str)
    {
        return false;
    }

    char* line_end = mem->mov_str;
    s32 line_length = line_end - line_start;

    if (line_length > 0)
    {
        StringCchCopyNA(mem->line_buf, SVR_INI_LINE_BUF_SIZE, line_start, line_length); 
    }

    return true;
}

bool svr_read_ini(SvrIniMem* mem, SvrIniLine* line, SvrIniTokenType* token_type)
{
    while (svr_read_ini_line(mem))
    {
        ini_parse_line(mem->line_buf, line, token_type);

        if (*token_type != SVR_INI_OTHER)
        {
            return true;
        }
    }

    return false;
}

void svr_close_ini(SvrIniMem* mem)
{
    free(mem->mem);
}
