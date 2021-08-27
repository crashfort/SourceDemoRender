#include "svr_vdf.h"
#include <Windows.h>
#include <strsafe.h>
#include <assert.h>

s32 vdf_is_newline(const char* seq)
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

bool vdf_is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

void vdf_parse_line(char* line_buf, SvrVdfLine* vdf_line, SvrVdfTokenType* type)
{
    char* ptr = line_buf;
    bool in_quote = false;

    char* token_start[2] = { NULL, NULL };
    char* token_end[2] = { NULL, NULL };
    s32 token_index = 0;
    SvrVdfTokenType token_type = SVR_VDF_OTHER;

    for (; *ptr != 0; ptr++)
    {
        if (!in_quote && vdf_is_whitespace(*ptr))
        {
            continue;
        }

        else if (*ptr == '\\' && *(ptr + 1) == '\\')
        {
            ptr++;
        }

        else if (*ptr == '\\' && *(ptr + 1) == '\"')
        {
            ptr++;
        }

        // We don't want escaped quotes within a quoted string.
        else if (*ptr == '\"')
        {
            if (!in_quote)
            {
                ptr++;
                token_start[token_index] = ptr;
            }

            else
            {
                if (token_index == 0)
                {
                    token_type = SVR_VDF_GROUP_TITLE;
                }

                else
                {
                    token_type = SVR_VDF_KV;
                }

                token_end[token_index] = ptr;
                token_index++;
            }

            in_quote = !in_quote;
        }
    }

    vdf_line->title[0] = 0;
    vdf_line->value[0] = 0;

    *type = token_type;

    switch (token_type)
    {
        case SVR_VDF_GROUP_TITLE:
        {
            s32 title_length = token_end[0] - token_start[0];

            StringCchCopyNA(vdf_line->title, SVR_VDF_TOKEN_BUF_SIZE, token_start[0], title_length);
            break;
        }

        case SVR_VDF_KV:
        {
            s32 title_length = token_end[0] - token_start[0];
            s32 value_length = token_end[1] - token_start[1];

            StringCchCopyNA(vdf_line->title, SVR_VDF_TOKEN_BUF_SIZE, token_start[0], title_length);
            StringCchCopyNA(vdf_line->value, SVR_VDF_TOKEN_BUF_SIZE, token_start[1], value_length);

            break;
        }
    }
}

SvrVdfLine svr_alloc_vdf_line()
{
    SvrVdfLine ret;
    ret.title = (char*)malloc(SVR_VDF_TOKEN_BUF_SIZE);
    ret.value = (char*)malloc(SVR_VDF_TOKEN_BUF_SIZE);

    return ret;
}

void svr_free_vdf_line(SvrVdfLine* line)
{
    free(line->title);
    free(line->value);
}

bool svr_open_vdf_read(const char* path, SvrVdfMem* mem)
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

        ReadFile(h, mem->mem, large.LowPart, NULL, NULL);

        mem->mov_str = (char*)mem->mem;
        mem->mov_str[large.LowPart] = 0;
        mem->line_buf = (char*)malloc(SVR_VDF_LINE_BUF_SIZE);

        ret = true;
    }

    CloseHandle(h);

    return ret;
}

bool svr_read_vdf_line(SvrVdfMem* mem)
{
    if (*mem->mov_str == 0)
    {
        return false;
    }

    char* line_start = mem->mov_str;

    // Skip all blank lines.

    for (; *mem->mov_str != 0;)
    {
        if (s32 nl = vdf_is_newline(mem->mov_str))
        {
            char* line_end = mem->mov_str;
            s32 line_length = line_end - line_start;

            if (line_length > 0)
            {
                StringCchCopyNA(mem->line_buf, SVR_VDF_LINE_BUF_SIZE, line_start, line_length); 
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

    return false;
}

bool svr_read_vdf(SvrVdfMem* mem, SvrVdfLine* line, SvrVdfTokenType* token_type)
{
    while (svr_read_vdf_line(mem))
    {
        vdf_parse_line(mem->line_buf, line, token_type);

        if (*token_type != SVR_VDF_OTHER)
        {
            return true;
        }
    }

    return false;
}

void svr_close_vdf(SvrVdfMem* mem)
{
    free(mem->mem);
}
