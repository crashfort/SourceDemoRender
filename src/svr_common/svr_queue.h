#pragma once
#include "svr_common.h"
#include "svr_fifo.h"

// Dynamic FIFO queue.

template <class T>
struct SvrDynQueue
{
    SvrDynFifo* fifo;

    inline void init(s32 init_capacity)
    {
        fifo = svr_fifo_alloc(init_capacity, sizeof(T));
    }

    inline void free()
    {
        svr_fifo_free(&fifo);
    }

    // Push a single item to the back.
    inline void push(T* item)
    {
        push_range(item, 1);
    }

    // Pop a single item frm the front.
    inline bool pull(T* item)
    {
        return pull_range(item, 1);
    }

    // Push many items to the back.
    inline void push_range(T* items, s32 num)
    {
        svr_fifo_write(fifo, items, num);
    }

    // Pull many items from the front.
    inline bool pull_range(T* dest, s32 num)
    {
        if (num > svr_fifo_can_read(fifo))
        {
            return false;
        }

        svr_fifo_read(fifo, dest, num);
        return true;
    }

    inline s32 size()
    {
        return svr_fifo_can_read(fifo);
    }

    inline void clear()
    {
        svr_fifo_reset(fifo);
    }
};
