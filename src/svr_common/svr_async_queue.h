#pragma once
#include "svr_common.h"
#include "svr_alloc.h"
#include "svr_atom.h"

// This is based on https://github.com/rigtorp/MPMCQueue by Erik Rigtorp!
// Safe for several threads to push and pull.

template <class T>
struct __declspec(align(SVR_CPU_CACHE_SIZE)) SvrAsyncQueueItem
{
    SvrAtom32 turn_;
    SVR_STRUCT_PADDING(4);

    T value;
};

template <class T>
struct SvrAsyncQueue
{
    SvrAsyncQueueItem<T>* items_;
    s32 buffer_capacity_;

    SVR_THREAD_PADDING();

    SvrAtom32 head_;

    SVR_THREAD_PADDING();

    SvrAtom32 tail_;

    SVR_THREAD_PADDING();

    inline s32 queue_idx(s32 i)
    {
        return i % buffer_capacity_;
    }

    inline s32 queue_turn(s32 i)
    {
        return i / buffer_capacity_;
    }

    inline void init(s32 capacity)
    {
        items_ = (SvrAsyncQueueItem<T>*)svr_zalloc(sizeof(SvrAsyncQueueItem<T>) * capacity);
        buffer_capacity_ = capacity;
    }

    inline void free()
    {
        svr_maybe_free((void**)&items_);
    }

    inline bool push(T* in_item)
    {
        s32 head = svr_atom_load(&head_);

        bool ret;

        while (true)
        {
            SvrAsyncQueueItem<T>* item = &items_[queue_idx(head)];

            if (queue_turn(head) * 2 == svr_atom_load(&item->turn_))
            {
                if (svr_atom_cmpxchg(&head_, &head, head + 1))
                {
                    item->value = *in_item;

                    svr_atom_store(&item->turn_, queue_turn(head) * 2 + 1);
                    ret = true;
                    break;
                }
            }

            else
            {
                s32 prev_head = head;
                head = svr_atom_load(&head_);

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
        s32 tail = svr_atom_load(&tail_);

        while (true)
        {
            SvrAsyncQueueItem<T>* item = &items_[queue_idx(tail)];

            if (queue_turn(tail) * 2 + 1 == svr_atom_load(&item->turn_))
            {
                if (svr_atom_cmpxchg(&tail_, &tail, tail + 1))
                {
                    *out_item = item->value;
                    svr_atom_store(&item->turn_, queue_turn(tail) * 2 + 2);
                    return true;
                }
            }

            else
            {
                s32 prev_tail = tail;
                tail = svr_atom_load(&tail_);

                if (tail == prev_tail)
                {
                    break;
                }
            }
        }

        return false;
    }
};
