#pragma once
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include "stb_sprintf.h"

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using wchar = wchar_t;

#define SVR_ARRAY_SIZE(A) (s32)(sizeof(A) / sizeof(A[0]))

#define SVR_FROM_MB(V) (V / 1024LL / 1024LL)

#define SVR_CAT_PART(A, B) A ## B
#define SVR_CAT(A, B) SVR_CAT_PART(A, B)

#define SVR_CPU_CACHE_SIZE 64

// For data separation between threads in the same structure.
#define SVR_THREAD_PADDING() u8 SVR_CAT(thread_padding_, __LINE__)[SVR_CPU_CACHE_SIZE]
#define SVR_STRUCT_PADDING(S) u8 SVR_CAT(struct_padding_, __LINE__)[S]

#define SVR_STR_CAT1(X) #X
#define SVR_STR_CAT(X) SVR_STR_CAT1(X)
#define SVR_FILE_LOCATION __FILE__ ":" SVR_STR_CAT(__LINE__)

#define SVR_ALLOCA(T) (T*)_alloca(sizeof(T))
#define SVR_ALLOCA_NUM(T, NUM) (T*)_alloca(sizeof(T) * NUM)

// Format to buffer with size restriction.
#define SVR_SNPRINTF(BUF, FORMAT, ...) stbsp_snprintf((BUF), SVR_ARRAY_SIZE((BUF)), FORMAT, __VA_ARGS__)
#define SVR_VSNPRINTF(BUF, FORMAT, VA) stbsp_vsnprintf((BUF), SVR_ARRAY_SIZE((BUF)), FORMAT, VA)

#define SVR_COPY_STRING(SOURCE, DEST) svr_copy_string((SOURCE), (DEST), SVR_ARRAY_SIZE((DEST)))

#define SVR_BIT(N) (1 << (N))

#ifdef _WIN64
#define SVR_IS_X64() true
#define SVR_IS_X86() false
#else
#define SVR_IS_X64() false
#define SVR_IS_X86() true
#endif

#ifdef _WIN64
#define SVR_ARCH_STRING "x64"
#else
#define SVR_ARCH_STRING "x86"
#endif

struct SvrVec2I
{
    s32 x;
    s32 y;
};

struct SvrVec4I
{
    s32 x;
    s32 y;
    s32 z;
    s32 w;
};

struct SvrVec3
{
    float x;
    float y;
    float z;
};

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
inline void svr_clamp(T* v, T min, T max)
{
    // This generates better code than using branches.
    *v = svr_min(svr_max(*v, min), max);
}

// Release a COM based object.
void svr_release(struct IUnknown* p);

// Maybe release a COM based object.
template <class T>
inline void svr_maybe_release(T** ptr)
{
    if (*ptr)
    {
        svr_release(*ptr);
    }

    *ptr = NULL;
}

// Maybe release a HANDLE based object.
void svr_maybe_close_handle(void** h);

void svr_maybe_free(void** addr);

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

s32 svr_to_utf16(const char* value, s32 value_length, wchar* buf, s32 buf_chars);

bool svr_is_sorted(s32* idxs, s32 num);
bool svr_are_idxs_unique(s32* idxs, s32 num);
void svr_check_all_mask(bool* mask, s32 num, bool* all_false, bool* all_true);

using SvrReadFileFlags = u32;

enum /* SvrReadFileFlags */
{
    SVR_READ_FILE_FLAGS_NEW_LINE = 1 << 0, // End with a new line.
};

char* svr_read_file_as_string(const char* path, SvrReadFileFlags flags);

const char* svr_read_line(const char* start, char* dest, s32 dest_size);

s32 svr_is_newline(const char* seq);
bool svr_is_whitespace(char c);
const char* svr_advance_until_after_whitespace(const char* text);
const char* svr_advance_until_whitespace(const char* text);
const char* svr_advance_until_char(const char* text, char c);
const char* svr_advance_quote(const char* text);
const char* svr_advance_string(bool quoted, const char* text);
const char* svr_extract_string(const char* text, char* dest, s32 dest_size);

void svr_unescape_path(const char* buf, char* dest, s32 dest_size);

bool svr_idx_in_range(s32 idx, s32 size);

struct SvrSplitTime
{
    s32 hours;
    s32 minutes;
    s32 seconds;
    s32 millis;
};

// Split input in microseconds to individual components.
SvrSplitTime svr_split_time(s64 us);

// Scale an integer from one range to another.
// This will round to the nearest result, example:
// svr_rescale(16667, 1000, 1000000) results in 17.
// svr_rescale(16444, 1000, 1000000) results in 16.
s64 svr_rescale(s64 a, s64 b, s64 c);

bool svr_check_all_true(bool* opts, s32 num);
bool svr_check_one_true(bool* opts, s32 num);
s32 svr_count_num_true(bool* opts, s32 num);
s32 svr_count_set_bits(u32 bits);

bool svr_does_file_exist(const char* path);

void svr_trim_right(char* buf, s32 length);
