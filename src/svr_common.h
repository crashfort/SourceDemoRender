#pragma once
#include <stdint.h>

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

const s32 SVR_VERSION = 34;

// Used by launcher and injector as parameter for svr_init_standalone in svr_game.dll.
struct SvrGameInitData
{
    const char* svr_path;
    u32 app_id;
};

// Steam app ids of the games we support.
const u32 SVR_GAME_CSS = 240;
const u32 SVR_GAME_CSGO = 730;

template <class T>
inline void svr_clamp(T* v, T min, T max)
{
    if (*v < min) *v = min;
    if (*v > max) *v = max;
}

template <class T>
inline float svr_max(T a, T b)
{
    return a > b ? a : b;
}

template <class T>
inline float svr_min(T a, T b)
{
    return a < b ? a : b;
}

template <class T>
inline void svr_maybe_release(T** ptr)
{
    if (*ptr) (*ptr)->Release();
    *ptr = NULL;
}

template <class T>
inline void svr_swap_ptrs(T** first, T** second)
{
    T* temp = *first;
    *first = *second;
    *second = temp;
}

template <class T>
inline void svr_maybe_free(T** ptr)
{
    void free(void*);
    if (*ptr) free(*ptr);
    *ptr = NULL;
}
