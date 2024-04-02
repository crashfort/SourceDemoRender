#pragma once
#include "svr_common.h"
#include <deque>
#include <algorithm>

// Dynamic FIFO queue.

template <class T>
struct SvrDynQueue
{
    std::deque<T> q;

    inline void init(s32 init_capacity)
    {
    }

    inline void free()
    {
        q.~deque();
    }

    // Push a single item to the back.
    inline void push(T* item)
    {
        q.push_back(*item);
    }

    // Pop a single item frm the front.
    inline bool pull(T* item)
    {
        if (q.size() == 0)
        {
            return false;
        }

        *item = q.front();
        q.pop_front();

        return true;
    }

    // Push many items to the back.
    inline void push_range(T* items, s32 num)
    {
        q.insert(q.end(), items, items + num);
    }

    // Pull many items from the front.
    inline bool pull_range(T* dest, s32 num)
    {
        if (num > q.size())
        {
            return false;
        }

        std::copy(q.begin(), q.begin() + num, dest);
        q.erase(q.begin(), q.begin() + num);
        return true;
    }

    inline s32 size()
    {
        return q.size();
    }

    inline void clear()
    {
        q.clear();
    }
};
