#pragma once
#include "svr_common.h"
#include <Windows.h>
#include <queue>

// Lock based queue.
// Safe for several threads to push and pull.
// During profiling during production, this was equally fast as SvrAsyncQueue and SvrAsyncStream.
// This has the additional benefit of keeping the code a lot simpler, and also has no chance
// of achieving the potential circular queue overflow problem which is difficult to handle without a huge mess.
// Even if you manage to handle the overflow problem, you now have a bottleneck problem instead where the writer is clearly faster than the reader.
// The things we store in these are now extremely large, so we just keep on growing since the order is very important.

template <class T>
struct SvrLockedQueue
{
    std::queue<T> items;
    SRWLOCK lock;

    inline void init(s32 capacity)
    {
    }

    inline void free()
    {
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

        if (items.size() == 0)
        {
            // Nothing to pull.
            goto rexit;
        }

        *item = items.front();
        items.pop();

        ret = true;

    rexit:
        ReleaseSRWLockExclusive(&lock);
        return ret;
    }
};
