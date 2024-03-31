#pragma once
#include "svr_common.h"
#include "svr_array.h"
#include <Windows.h>

// Lock based dynamic array.
// Safe for several threads to push and pull.

template <class T>
struct SvrLockedArray
{
    SvrDynArray<T> items;
    SRWLOCK lock;

    inline void init(s32 init_capacity)
    {
        items.init(init_capacity);
    }

    inline void free()
    {
        items.free();
    }

    inline void push(T* item)
    {
        AcquireSRWLockExclusive(&lock);
        items.push(*item);
        ReleaseSRWLockExclusive(&lock);
    }

    inline bool pull(T* item)
    {
        bool ret = false;

        AcquireSRWLockExclusive(&lock);

        if (items.size == 0)
        {
            // Nothing to pull.
            goto rexit;
        }

        *item = items[items.size - 1];
        items.size--;

        ret = true;

    rexit:
        ReleaseSRWLockExclusive(&lock);
        return ret;
    }
};
