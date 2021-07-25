#pragma once
#include "svr_common.h"

struct SvrMemoryRange
{
    void* start;
    s32 used;
    s32 size;

    // Pushes some size and returns the start of that. Works well with pods and reasonable types but if you have a constructor or virtual nonsense then you need
    // to use placement new on the returned address like this:
    // new (push(sizeof(HwndDropTarget))) HwndDropTarget;
    void* push(s32 size_in_bytes);

    void* get_head();

    bool can_size_fit(s32 size_in_bytes);

    SvrMemoryRange divide(s32 size_in_bytes);
};
