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

const s32 SVR_VERSION = 33;

// Used by launcher and injector as parameter for svr_init_standalone in svr_game.dll.
struct SvrGameInitData
{
    const char* svr_path;
    u32 app_id;
};

// Steam app ids of the games we support.
const u32 SVR_GAME_CSS = 240;
// const u32 SVR_GAME_CSGO = 730;

inline void svr_clamp(s32& v, s32 min, s32 max) { if (v < min) v = min; if (v > max) v = max; }
inline void svr_clamp(float& v, float min, float max) { if (v < min) v = min; if (v > max) v = max; }

inline float svr_max(float a, float b) { return a > b ? a : b; }
inline float svr_max(s32 a, s32 b) { return a > b ? a : b; }
