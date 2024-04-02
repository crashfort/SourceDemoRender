#pragma once
#include "svr_common.h"
#include "svr_queue.h"
#include <Windows.h>

// Lock based queue.
// Safe for several threads to push and pull.
// During profiling during production, this was equally fast as SvrAsyncQueue and SvrAsyncStream.
// This also has no chance of achieving the potential circular queue overflow problem which is difficult to handle without a huge mess.
// Even if you manage to handle the overflow problem, you now have a bottleneck problem instead where the writer is clearly faster than the reader.
// The things we store in these are not extremely large, so we just keep on growing since the order is very important.

template <class T>
struct SvrLockedQueue
{
    SvrDynQueue<T> items;
    SRWLOCK lock;

    inline void init(s32 init_capacity)
    {
        items.init(init_capacity);
    }

    inline void free()
    {
        items.free();
    }

    // Pushes to the back.
    inline void push(T* item)
    {
        AcquireSRWLockExclusive(&lock);
        items.push(item);
        ReleaseSRWLockExclusive(&lock);
    }

    // Pops from the front.
    inline bool pull(T* item)
    {
        bool ret = false;

        AcquireSRWLockExclusive(&lock);

        if (items.size() == 0)
        {
            // Nothing to pull.
            goto rexit;
        }

        items.pull(item);

        ret = true;

    rexit:
        ReleaseSRWLockExclusive(&lock);
        return ret;
    }
};
