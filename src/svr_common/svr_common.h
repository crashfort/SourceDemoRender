#pragma once
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using wchar = wchar_t;

#define SVR_ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))

#define SVR_FROM_MB(V) (V / 1024LL / 1024LL)

#define SVR_CAT_PART(A, B) A ## B
#define SVR_CAT(A, B) SVR_CAT_PART(A, B)

#define SVR_CPU_CACHE_SIZE 64

// For data separation between threads in the same structure.
#define SVR_THREAD_PADDING() u8 SVR_CAT(thread_padding_, __LINE__)[SVR_CPU_CACHE_SIZE]

#define SVR_STR_CAT1(X) #X
#define SVR_STR_CAT(X) SVR_STR_CAT1(X)
#define SVR_FILE_LOCATION __FILE__ ":" SVR_STR_CAT(__LINE__)

#define SVR_ALLOCA(T) (T*)_alloca(sizeof(T))
#define SVR_ALLOCA_NUM(T, NUM) (T*)_alloca(sizeof(T) * NUM)

// Format to buffer with size restriction.
#define SVR_SNPRINTF(BUF, FORMAT, ...) snprintf((BUF), SVR_ARRAY_SIZE((BUF)), FORMAT, __VA_ARGS__)

// Should be same type from Steam API.
using SteamAppId = u32;

// Used by launcher and injector as parameter for the init exports in svr_game.dll (see bottom of game_standalone.cpp).
struct SvrGameInitData
{
    const char* svr_path;
    SteamAppId app_id;
};

struct SvrVec2I
{
    s32 x;
    s32 y;
};

template <class T>
inline void svr_clamp(T* v, T min, T max)
{
    if (*v < min) *v = min;
    if (*v > max) *v = max;
}

template <class T>
inline T svr_max(T a, T b)
{
    return a > b ? a : b;
}

template <class T>
inline T svr_min(T a, T b)
{
    return a < b ? a : b;
}

template <class T>
inline void svr_maybe_release(T** ptr)
{
    if (*ptr) (*ptr)->Release();
    *ptr = NULL;
}

// Aligns up.
inline s32 svr_align32(s32 value, s32 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// Aligns up.
inline s64 svr_align64(s64 value, s64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

s32 svr_copy_string(const char* source, char* dest, s32 dest_chars);

// Temporary buffer formatting.
const char* svr_va(const char* format, ...);

bool svr_starts_with(const char* str, const char* prefix);
bool svr_ends_with(const char* str, const char* suffix);
