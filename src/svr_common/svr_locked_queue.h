#pragma once
#include "svr_common.h"
#include "svr_alloc.h"
#include "svr_atom.h"
#include <assert.h>
#include <Windows.h>

// Lock based queue.
// Safe for several threads to push and pull.

template <class T>
struct SvrLockedQueue
{
    T* items_;
    SRWLOCK lock_;
    s32 buffer_capacity_;

    s32 write_idx_;
    s32 read_idx_;

    inline void init(s32 capacity)
    {
        items_ = (T*)svr_alloc(sizeof(T) * capacity);
        buffer_capacity_ = capacity;
    }

    inline void free()
    {
        svr_maybe_free((void**)&items_);
    }

    inline bool push(T* item)
    {
        bool ret = false;

        AcquireSRWLockExclusive(&lock_);

        s32 next_idx = write_idx_ + 1;

        if (next_idx == buffer_capacity_)
        {
            next_idx = 0;
        }

        if (next_idx == read_idx_)
        {
            // We cannot add another element now. This is capacity minus 1. The read and write ends cannot be the same (as that would mean empty).
            goto rexit;
        }

        items_[write_idx_] = *item;

        write_idx_ = next_idx;

        ret = true;

    rexit:
        ReleaseSRWLockExclusive(&lock_);
        return ret;
    }

    inline bool pull(T* item)
    {
        bool ret = false;

        AcquireSRWLockExclusive(&lock_);

        if (read_idx_ == write_idx_)
        {
            // Nothing to pull.
            goto rexit;
        }

        *item = items_[read_idx_];

        s32 next_idx = read_idx_ + 1;

        if (next_idx == buffer_capacity_)
        {
            next_idx = 0;
        }

        read_idx_ = next_idx;

        ret = true;

    rexit:
        ReleaseSRWLockExclusive(&lock_);
        return ret;
    }
};
