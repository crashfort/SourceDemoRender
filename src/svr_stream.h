#pragma once
#include "svr_common.h"
#include "svr_atom.h"
#include <malloc.h>

// This is based on https://github.com/rigtorp/SPSCQueue by Erik Rigtorp!
// Safe for 1 thread to push and for 1 thread to pull.
template <class T>
struct SvrAsyncStream
{
    SVR_THREAD_PADDING();

    SvrAtom32 head_;

    SVR_THREAD_PADDING();

    SvrAtom32 tail_;

    SVR_THREAD_PADDING();

    T* slots_;
    s32 buffer_capacity;

    inline void init(s32 capacity)
    {
        // The read and write ends cannot be the same (as then it would be empty).
        capacity += 1;

        // We don't use mem ranges in SVR so just use a dumb allocation instead for simplicity.
        // Doing it this way removes the pre and post cache region protection but whatever.
        slots_ = (T*)malloc(sizeof(T) * capacity);

        buffer_capacity = capacity;
    }

    // When every item is going to be readded.
    // The environment must be controlled and known for this to be called.
    inline void reset()
    {
        svr_atom_set(&head_, 0);
        svr_atom_set(&tail_, 0);
    }

    // Mem is not copied! Mem is only referenced!
    // Returns false if the buffer appears full.
    inline bool push(T* item)
    {
        s32 head = svr_atom_read(&head_);
        s32 next_head = head + 1;

        if (next_head == buffer_capacity)
        {
            next_head = 0;
        }

        if (next_head == svr_atom_load(&tail_))
        {
            // We cannot add another element now. This is capacity minus 1. The read and write ends cannot be the same (as that would mean empty).
            return false;
        }

        slots_[head] = *item;

        svr_atom_store(&head_, next_head);
        return true;
    }

    // Tries to get a pack from the reading end.
    // Returns true if there was something.
    inline bool pull(T* item)
    {
        s32 tail = svr_atom_read(&tail_);

        if (svr_atom_load(&head_) == tail)
        {
            // Nothing to pull.
            return false;
        }

        *item = slots_[tail];

        s32 next_tail = tail + 1;

        if (next_tail == buffer_capacity)
        {
            next_tail = 0;
        }

        svr_atom_store(&tail_, next_tail);
        return true;
    }

    inline s32 read_buffer_health()
    {
        s32 diff = svr_atom_load(&head_) - svr_atom_load(&tail_);

        if (diff < 0)
        {
            diff += buffer_capacity;
        }

        return diff;
    }

    inline bool is_buffer_full()
    {
        // The read and write ends cannot be the same (as then it would be empty).
        return read_buffer_health() == buffer_capacity - 1;
    }
};
