#pragma once
#include "svr_common.h"

// Goes through every relevant line in a vdf.

// How long one line can be in bytes.
const s32 SVR_VDF_LINE_BUF_SIZE = 32 * 1024;

// How long the title and value can be in bytes.
const s32 SVR_VDF_TOKEN_BUF_SIZE = 8 * 1024;

struct SvrVdfMem
{
    void* mem;
    char* mov_str;
    char* line_buf;
};

struct SvrVdfLine
{
    char* title;
    char* value;
};

// SvrVdfTokenType.
const s32 SVR_VDF_OTHER = 0;
const s32 SVR_VDF_GROUP_TITLE = 1;
const s32 SVR_VDF_KV = 2;

SvrVdfLine svr_alloc_vdf_line();
void svr_free_vdf_line(SvrVdfLine& line);

bool svr_open_vdf_read(const char* path, SvrVdfMem* mem);

// Call this until it returns false.
bool svr_read_vdf(SvrVdfMem& mem, SvrVdfLine* line, /* SvrVdfTokenType */ s32* token_type);

void svr_close_vdf(SvrVdfMem mem);
