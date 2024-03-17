#include "svr_alloc.h"
#include <stdio.h>
#include <string.h>

void* svr_alloc(s32 size)
{
    return malloc(size);
}

void* svr_zalloc(s32 size)
{
    void* m = svr_alloc(size);
    memset(m, 0, size);
    return m;
}

void* svr_realloc(void* p, s32 size)
{
    return realloc(p, size);
}

wchar* svr_dup_wstr(const wchar* source)
{
    return wcsdup(source);
}

char* svr_dup_str(const char* source)
{
    return strdup(source);
}

void* svr_align_alloc(s32 size, s32 align)
{
    return _aligned_malloc(size, align);
}

void svr_free(void* addr)
{
    free(addr);
}

void svr_align_free(void* addr, s32 align)
{
    _aligned_free(addr);
}
