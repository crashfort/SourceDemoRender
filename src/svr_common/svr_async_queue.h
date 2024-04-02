#pragma once
#include "svr_common.h"
#include "svr_alloc.h"
#include "svr_atom.h"

// This is based on https://github.com/rigtorp/MPMCQueue by Erik Rigtorp!
// Safe for several threads to push and pull.

template <class T>
struct __declspec(align(SVR_CPU_CACHE_SIZE)) SvrAsyncQueueItem
{
    SvrAtom32 turn;
    SVR_STRUCT_PADDING(4);

    T value;
};

template <class T>
struct SvrAsyncQueue
{
    SvrAsyncQueueItem<T>* items;
    s32 buffer_capacity;

    SVR_THREAD_PADDING();

    SvrAtom32 head_atom;

    SVR_THREAD_PADDING();

    SvrAtom32 tail_atom;

    SVR_THREAD_PADDING();

    inline s32 queue_idx(s32 i)
    {
        return i % buffer_capacity;
    }

    inline s32 queue_turn(s32 i)
    {
        return i / buffer_capacity;
    }

    inline void init(s32 capacity)
    {
        items = (SvrAsyncQueueItem<T>*)svr_zalloc(sizeof(SvrAsyncQueueItem<T>) * capacity);
        buffer_capacity = capacity;
    }

    inline void free()
    {
        svr_maybe_free((void**)&items);
    }

    inline bool push(T* in_item)
    {
        s32 head = svr_atom_load(&head_atom);

        bool ret;

        while (true)
        {
            SvrAsyncQueueItem<T>* item = &items[queue_idx(head)];

            if (queue_turn(head) * 2 == svr_atom_load(&item->turn))
            {
                if (svr_atom_cmpxchg(&head_atom, &head, head + 1))
                {
                    item->value = *in_item;

                    svr_atom_store(&item->turn, queue_turn(head) * 2 + 1);
                    ret = true;
                    break;
                }
            }

            else
            {
                s32 prev_head = head;
                head = svr_atom_load(&head_atom);

                if (head == prev_head)
                {
                    // Too many tasks! We have wrapped around.
                    ret = false;
                    break;
                }
            }
        }

        return ret;
    }

    inline bool pull(T* out_item)
    {
        s32 tail = svr_atom_load(&tail_atom);

        while (true)
        {
            SvrAsyncQueueItem<T>* item = &items[queue_idx(tail)];

            if (queue_turn(tail) * 2 + 1 == svr_atom_load(&item->turn))
            {
                if (svr_atom_cmpxchg(&tail_atom, &tail, tail + 1))
                {
                    *out_item = item->value;
                    svr_atom_store(&item->turn, queue_turn(tail) * 2 + 2);
                    return true;
                }
            }

            else
            {
                s32 prev_tail = tail;
                tail = svr_atom_load(&tail_atom);

                if (tail == prev_tail)
                {
                    break;
                }
            }
        }

        return false;
    }
};
