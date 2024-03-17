#pragma once
#include "svr_common.h"

void* svr_alloc(s32 size);
void* svr_zalloc(s32 size); // Zero init alloc.
void* svr_realloc(void* p, s32 size);
wchar* svr_dup_wstr(const wchar* source);
char* svr_dup_str(const char* source);
void* svr_align_alloc(s32 size, s32 align);
void svr_free(void* addr);
void svr_align_free(void* addr, s32 align);

// Easier to type when you need to allocate structures.
#define SVR_ZALLOC(T) (T*)svr_zalloc(sizeof(T))
#define SVR_ZALLOC_NUM(T, NUM) (T*)svr_zalloc(sizeof(T) * NUM)

#define SVR_ALLOCA(T) (T*)_alloca(sizeof(T))
#define SVR_ALLOCA_NUM(T, NUM) (T*)_alloca(sizeof(T) * NUM)
