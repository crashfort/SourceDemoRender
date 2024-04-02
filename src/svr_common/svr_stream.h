#pragma once
#include "svr_common.h"
#include "svr_atom.h"
#include "svr_alloc.h"

// This is based on https://github.com/rigtorp/SPSCQueue by Erik Rigtorp!
// Safe for 1 thread to push and for 1 thread to pull.
template <class T>
struct SvrAsyncStream
{
    SVR_THREAD_PADDING();

    SvrAtom32 head_atom;

    SVR_THREAD_PADDING();

    SvrAtom32 tail_atom;

    SVR_THREAD_PADDING();

    T* slots;
    s32 buffer_capacity;

    inline void init(s32 capacity)
    {
        // The read and write ends cannot be the same (as then it would be empty).
        capacity += 1;

        // We don't use mem ranges in SVR so just use a dumb allocation instead for simplicity.
        // Doing it this way removes the pre and post cache region protection but whatever.
        slots = (T*)svr_alloc(sizeof(T) * capacity);

        buffer_capacity = capacity;
    }

    inline void free()
    {
        svr_maybe_free((void**)&slots);
    }

    // Mem is not copied! Mem is only referenced!
    // Returns false if the buffer appears full.
    inline bool push(T* item)
    {
        s32 head = svr_atom_load(&head_atom);
        s32 next_head = head + 1;

        if (next_head == buffer_capacity)
        {
            next_head = 0;
        }

        if (next_head == svr_atom_load(&tail_atom))
        {
            // We cannot add another element now. This is capacity minus 1. The read and write ends cannot be the same (as that would mean empty).
            return false;
        }

        slots[head] = *item;

        svr_atom_store(&head_atom, next_head);
        return true;
    }

    // Tries to get a pack from the reading end.
    // Returns true if there was something.
    inline bool pull(T* item)
    {
        s32 tail = svr_atom_load(&tail_atom);

        if (svr_atom_load(&head_atom) == tail)
        {
            // Nothing to pull.
            return false;
        }

        *item = slots[tail];

        s32 next_tail = tail + 1;

        if (next_tail == buffer_capacity)
        {
            next_tail = 0;
        }

        svr_atom_store(&tail_atom, next_tail);
        return true;
    }
};
