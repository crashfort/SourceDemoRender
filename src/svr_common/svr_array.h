#pragma once
#include "svr_common.h"
#include <assert.h>
#include <string.h>

// Dynamic array that increases its size when needed. Can only have sane data and not C++ class nonsense.
template <class T>
struct SvrDynArray
{
    T* mem;
    s32 size; // How many items there are.
    s32 capacity; // How many items there can be.
    s32 grow_align; // How much to grow by when needed.

    T& operator[](s32 idx)
    {
        assert(svr_idx_in_range(idx, size));
        return mem[idx];
    }

    inline void init(s32 initial_max_size)
    {
        mem = NULL;
        size = 0;
        capacity = 0;

        if (initial_max_size > 0)
        {
            change_capacity(initial_max_size);
        }
    }

    // Init needs to be called again after.
    inline void free()
    {
        svr_maybe_free((void**)&mem);

        size = 0;
        capacity = 0;
    }

    inline void change_capacity(s32 max_items)
    {
        if (max_items > capacity)
        {
            mem = (T*)svr_realloc(mem, sizeof(T) * max_items);
            capacity = max_items;
        }
    }

    inline void expand_if_needed(s32 wanted)
    {
        if (wanted > capacity)
        {
            // Default to grow by aligning up to the nearest multiple of 8 works pretty good, but not in all contexts.
            // We allow this to be settable because in some contexts (like in the UI) where there are loads of stuff we don't want to reallocate that often.
            s32 align = grow_align;

            if (align == 0)
            {
                align = 8;
            }

            s32 new_capacity = svr_align32(wanted, align);
            change_capacity(new_capacity);
        }
    }

    inline void push(const T& item)
    {
        expand_if_needed(size + 1);

        // Add to back.
        memcpy(mem + size, &item, sizeof(T));
        size++;
    }

    // Push a number of the same thing.
    inline void push_num(const T& item, s32 num)
    {
        expand_if_needed(size + num);

        // Add to back.
        for (s32 i = 0; i < num; i++)
        {
            memcpy(mem + size, &item, sizeof(T));
            size++;
        }
    }

    inline T* emplace()
    {
        expand_if_needed(size + 1);

        // Add to back.
        T* write_pos = mem + size;

        size++;

        return write_pos;
    }

    inline T* emplace_zero()
    {
        expand_if_needed(size + 1);

        // Add to back.
        T* write_pos = mem + size;

        memset(write_pos, 0, sizeof(T));

        size++;

        return write_pos;
    }

    inline void insert(s32 idx, const T& item)
    {
        assert(idx >= 0);
        assert(idx <= size);

        expand_if_needed(size + 1);

        if (idx == size)
        {
            push(item);
            return;
        }

        s32 new_size = size + 1;
        T* insert_at = mem + idx;

        // Move what we have to make room to insert.
        memmove(insert_at + 1, insert_at, (size - idx) * sizeof(T));
        *insert_at = item; // Insert the thing.

        size++;
    }

    inline void insert_range(s32 idx, const T* items, s32 num)
    {
        if (num == 0)
        {
            return; // Nothing to do big guy.
        }

        assert(idx >= 0);
        assert(idx <= size);

        expand_if_needed(size + num);

        // Insert at back.
        if (idx == size)
        {
            memcpy(mem + size, items, sizeof(T) * num);
            size += num;
            return;
        }

        s32 new_size = size + num;
        T* insert_at = mem + idx;

        // Move what we have to make room to insert.
        memmove(insert_at + num, insert_at, (size - idx) * sizeof(T));
        memcpy(insert_at, items, sizeof(T) * num); // Insert the thing.

        size += num;
    }

    inline void remove_all_of(T* match)
    {
        while (true)
        {
            // Unescape stuff if we have any.

            s32 idx = find_index_of(match);

            if (idx == -1)
            {
                break;
            }

            remove_index(idx);
        }
    }

    // This will shift every item behind and keep order.
    // If order is not needed, remove_index is a lot faster.
    inline void remove_index_keep_order(s32 i)
    {
        assert(i >= 0);
        assert(size > 0);
        assert(i < size);

        if (i == size - 1)
        {
            #ifdef SVR_ARRAY_ZERO_ON_REMOVE
            memset(mem + size - 1, 0, sizeof(T));
            #endif

            size--;
            return;
        }

        s32 new_size = size - 1;
        T* remove_at = mem + i;

        memmove(remove_at, remove_at + 1, (new_size - i) * sizeof(T));

        #ifdef SVR_ARRAY_ZERO_ON_REMOVE
        memset(remove_at + (new_size - i), 0, sizeof(T));
        #endif

        size--;
    }

    inline s32 find_index_of(T* ref)
    {
        return find_index_of(ref, 0);
    }

    // Faster version if you are searching in a loop, so you can continue where you left off.
    inline s32 find_index_of(T* ref, s32 start_from)
    {
        for (s32 i = start_from; i < size; i++)
        {
            T* e = &mem[i];

            if (!memcmp(e, ref, sizeof(T)))
            {
                return i;
            }
        }

        return -1;
    }

    // Find first index of a linear sequence of items. Only useful for strings I guess?
    inline s32 find_index_of_sequence(T* ref, s32 num)
    {
        return find_index_of_sequence(ref, num, 0);
    }

    // Find first index of a linear sequence of items. Only useful for strings I guess?
    inline s32 find_index_of_sequence(T* ref, s32 num, s32 start_from)
    {
        assert(num <= size);

        for (s32 i = start_from; i < (size - num); i++)
        {
            T* e = &mem[i];

            if (!memcmp(e, ref, sizeof(T) * num))
            {
                return i;
            }
        }

        return -1;
    }

    // This will swap positions with the last item, breaking order.
    // Use remove_indexes if you need to remove many, as this will not work for that (because indexes change if you move things around).
    inline void remove_index(s32 i)
    {
        assert(i >= 0);
        assert(size > 0);

        // Swap the found index with the last index and reduce item count.

        if (i != size - 1)
        {
            memcpy(mem + i, mem + size - 1, sizeof(T));
        }

        #ifdef SVR_ARRAY_ZERO_ON_REMOVE
        memset(mem + size - 1, 0, sizeof(T));
        #endif

        size--;
    }

    // Shift all later elements to the start position and reduce size.
    inline void remove_range(s32 start_idx, s32 num)
    {
        assert(size > 0);
        assert(start_idx + num <= size);

        if (num == 1)
        {
            remove_index_keep_order(start_idx);
            return;
        }

        if (start_idx == 0 && num == size)
        {
            #ifdef SVR_ARRAY_ZERO_ON_REMOVE
            memset(mem, 0, used_size_in_memory());
            #endif

            size = 0;
            return;
        }

        s32 end_idx = start_idx + num;
        s32 num_after_range = size - end_idx;

        memmove(mem + start_idx, mem + end_idx, sizeof(T) * num_after_range);

        #ifdef SVR_ARRAY_ZERO_ON_REMOVE
        // TODO If we decide to use this.
        #endif

        size -= num;
    }

    inline void remove_indexes(s32* idxs, s32 num)
    {
        assert(size > 0);
        assert(svr_are_idxs_unique(idxs, num));
        assert(svr_is_sorted(idxs, num));

        if (num == 0)
        {
            return;
        }

        if (num == 1)
        {
            remove_index_keep_order(idxs[0]);
            return;
        }

        else if (num == size)
        {
            #ifdef SVR_ARRAY_ZERO_ON_REMOVE
            memset(mem, 0, used_size_in_memory());
            #endif

            size = 0;
            return;
        }

        s32 next_keep = 0;
        s32 num_matches = 0;
        s32 next_idx = idxs[0];

        // Elements to keep are put in the start (or elements to remove are put in the back) and the size is reduced.
        // Since we are removing indexes, we have to check over every item, as this can be random access.

        for (s32 i = 0; i < size; i++)
        {
            if (i != next_idx)
            {
                // Don't copy to self.
                if (i != next_keep)
                {
                    memcpy(mem + next_keep, mem + i, sizeof(T));

                    #ifdef SVR_ARRAY_ZERO_ON_REMOVE
                    memset(mem + i, 0, sizeof(T));
                    #endif
                }

                // We have moved an item to the front, so advance to the next spot.
                next_keep++;
            }

            else
            {
                assert(num_matches < num);

                num_matches++;

                if (num_matches != num)
                {
                    next_idx = idxs[num_matches];
                }

                else
                {
                    next_idx = -1;
                }

                #ifdef SVR_ARRAY_ZERO_ON_REMOVE
                memset(mem + i, 0, sizeof(T));
                #endif
            }
        }

        size -= num_matches;
    }

    // Mask should be true for keep, false for remove.
    // This keeps order.
    inline void remove_all_match(bool* mask, s32 mask_size)
    {
        assert(size > 0);
        assert(mask_size == size);

        bool all_false;
        bool all_true;
        svr_check_all_mask(mask, size, &all_false, &all_true);

        // All should be kept.
        if (all_true)
        {
            return;
        }

        // All should be removed.
        if (all_false)
        {
            #ifdef SVR_ARRAY_ZERO_ON_REMOVE
            memset(mem, 0, used_size_in_memory());
            #endif

            size = 0;
            return;
        }

        s32 next_keep = 0;
        s32 num_matches = 0;

        // Elements to keep are put in the start (or elements to remove are put in the back) and the size is reduced.

        for (s32 i = 0; i < size; i++)
        {
            if (mask[i])
            {
                // Don't copy to self.
                if (i != next_keep)
                {
                    memcpy(mem + next_keep, mem + i, sizeof(T));

                    #ifdef SVR_ARRAY_ZERO_ON_REMOVE
                    memset(mem + i, 0, sizeof(T));
                    #endif
                }

                // We have moved an item to the front, so advance to the next spot.
                next_keep++;
            }

            else
            {
                num_matches++;

                #ifdef SVR_ARRAY_ZERO_ON_REMOVE
                memset(mem + i, 0, sizeof(T));
                #endif
            }
        }

        size -= num_matches;
    }

    // How many bytes are being occupied with actual data right now.
    inline s32 used_size_in_memory()
    {
        return sizeof(T) * size;
    }

    inline void copy_from(SvrDynArray<T>* other)
    {
        expand_if_needed(other->size);

        if (other->size > 0)
        {
            memcpy(mem, other->mem, sizeof(T) * other->size);
        }

        size = other->size;
        grow_align = other->grow_align;
    }

    inline T* back()
    {
        if (size == 0)
        {
            return NULL;
        }

        return mem + (size - 1);
    }
};
