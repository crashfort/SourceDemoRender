#include "svr_mem_range.h"
#include <assert.h>

void* SvrMemoryRange::push(s32 size_in_bytes)
{
    // Not enough room remaining!
    assert(can_size_fit(size_in_bytes));

    void* ret = get_head();

    used += size_in_bytes;

    return ret;
}

void* SvrMemoryRange::get_head()
{
    return (u8*)start + used;
}

bool SvrMemoryRange::can_size_fit(s32 size_in_bytes)
{
    return used + size_in_bytes <= size;
}

SvrMemoryRange SvrMemoryRange::divide(s32 size_in_bytes)
{
    SvrMemoryRange ret;
    ret.start = push(size_in_bytes);
    ret.used = 0;
    ret.size = size_in_bytes;

    return ret;
}
