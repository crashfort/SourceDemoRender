#pragma once
#include "svr_common.h"
#include "svr_atom.h"
#include "svr_mem_range.h"

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

    inline void init_with_range(SvrMemoryRange& range, s32 capacity)
    {
        // Multithreaded stuff, so pad both ends.

        capacity += 1;

        range.push(SVR_CPU_CACHE_SIZE);
        slots_ = (T*)range.push(sizeof(T) * capacity);
        range.push(SVR_CPU_CACHE_SIZE);

        buffer_capacity = capacity;
    }

    inline void reset()
    {
        svr_atom_set(&head_, 0);
        svr_atom_set(&tail_, 0);
    }

    // Mem is not copied! Mem is only referenced!
    // Size or extra don't have to be specified, they're just additional info to put in the pack. Just know to pull the pack the same way it was pushed.
    // Returns false if the buffer is full.
    inline bool push(const T& item)
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

        slots_[head] = item;

        svr_atom_store(&head_, next_head);
        return true;
    }

    // Tries to get a pack from the reading end.
    // The mem may have to be freed dependning on how it was created! It must be freed the same way it was allocated.
    // Returns false if there's nothing to pull and *pack is set to NULL.
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
