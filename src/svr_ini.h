#pragma once
#include "svr_common.h"

// Goes through every relevant line in a ini.
// We use ini now instead of json for two reasons: First, json is overly complicated to parse and libraries are overly complicated. Second, users get confused with the formatting rules
// and cases that include escaping a sequence of characters.

// How long one line can be in bytes.
const s32 SVR_INI_LINE_BUF_SIZE = 32 * 1024;

// How long the title and value can be in bytes.
const s32 SVR_INI_TOKEN_BUF_SIZE = 8 * 1024;

struct SvrIniMem
{
    void* mem;
    char* mov_str;
    char* line_buf;
};

struct SvrIniLine
{
    char* title;
    char* value;
};

using SvrIniTokenType = s32;
const SvrIniTokenType SVR_INI_OTHER = 0;
const SvrIniTokenType SVR_INI_KV = 1;

SvrIniLine svr_alloc_ini_line();
void svr_free_ini_line(SvrIniLine* line);

bool svr_open_ini_read(const char* path, SvrIniMem* mem);

// Call this until it returns false (or break the loop when needed).
bool svr_read_ini(SvrIniMem* mem, SvrIniLine* line, SvrIniTokenType* token_type);

void svr_close_ini(SvrIniMem* mem);
