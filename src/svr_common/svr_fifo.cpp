#include "svr_fifo.h"
#include "svr_alloc.h"
#include <string.h>
#include <assert.h>

// This is based on https://ffmpeg.org/doxygen/trunk/libavutil_2fifo_8c_source.html by the FFmpeg developers!

 // By default the FIFO can be auto-grown to 1MB.
#define AUTO_GROW_DEFAULT_BYTES (1024 * 1024)

struct SvrDynFifo
{
    u8* buffer;
    s32 elem_size; // Size of each item.
    s32 nb_elems; // Item capacity of buffer.
    s32 offset_r; // Read offset.
    s32 offset_w; // Write offset.
    s32 is_empty; // To distinguish the case if the read and write offsets are the same.
    s32 auto_grow_limit;
};

SvrDynFifo* svr_fifo_alloc(s32 nb_elems, s32 elem_size)
{
    void* buffer = NULL;

    if (nb_elems)
    {
        buffer = svr_realloc(NULL, nb_elems * elem_size);
    }

    SvrDynFifo* f = SVR_ZALLOC(SvrDynFifo);
    f->buffer = (u8*)buffer;
    f->nb_elems = nb_elems;
    f->elem_size = elem_size;
    f->is_empty = 1;
    f->auto_grow_limit = svr_max(AUTO_GROW_DEFAULT_BYTES / elem_size, 1);

    return f;
}

s32 svr_fifo_can_read(SvrDynFifo* f)
{
    if (f->offset_w <= f->offset_r && !f->is_empty)
    {
        return f->nb_elems - f->offset_r + f->offset_w;
    }

    return f->offset_w - f->offset_r;
}

s32 svr_fifo_can_write(SvrDynFifo* f)
{
    return f->nb_elems - svr_fifo_can_read(f);
}

s32 svr_fifo_grow(SvrDynFifo* f, s32 inc)
{
    if (inc > INT32_MAX - f->nb_elems)
    {
        return -1;
    }

    u8* tmp = (u8*)svr_realloc(f->buffer, (f->nb_elems + inc) * f->elem_size);

    f->buffer = tmp;

    // Move the data from the beginning of the ring buffer to the newly allocated space
    if (f->offset_w <= f->offset_r && !f->is_empty)
    {
        s32 copy = svr_min(inc, f->offset_w);

        memcpy(tmp + f->nb_elems * f->elem_size, tmp, copy * f->elem_size);

        if (copy < f->offset_w)
        {
            memmove(tmp, tmp + copy * f->elem_size, (f->offset_w - copy) * f->elem_size);
            f->offset_w -= copy;
        }

        else
        {
            f->offset_w = copy == inc ? 0 : f->nb_elems + copy;
        }
    }

    f->nb_elems += inc;

    return 0;
}

s32 svr_fifo_check_space(SvrDynFifo* f, s32 to_write)
{
    s32 can_write = svr_fifo_can_write(f);
    s32 need_grow = to_write > can_write ? to_write - can_write : 0;

    if (!need_grow)
    {
        return 0;
    }

    s32 can_grow = f->auto_grow_limit > f->nb_elems ? f->auto_grow_limit - f->nb_elems : 0;

    if (need_grow <= can_grow)
    {
        // Allocate a bit more than necessary, if we can.
        s32 inc = (need_grow < can_grow / 2) ? need_grow * 2 : can_grow;
        return svr_fifo_grow(f, inc);
    }

    return -1;
}

s32 svr_fifo_write_common(SvrDynFifo* f, u8* buf, s32* nb_elems)
{
    s32 to_write = *nb_elems;
    s32 offset_w;
    s32 ret = 0;

    ret = svr_fifo_check_space(f, to_write);

    if (ret < 0)
    {
        return ret;
    }

    offset_w = f->offset_w;

    while (to_write > 0)
    {
        s32 len = svr_min(f->nb_elems - offset_w, to_write);
        u8* write_ptr = f->buffer + offset_w * f->elem_size;

        memcpy(write_ptr, buf, len * f->elem_size);
        buf += len * f->elem_size;

        offset_w += len;

        if (offset_w >= f->nb_elems)
        {
            offset_w = 0;
        }

        to_write -= len;
    }

    f->offset_w = offset_w;

    if (*nb_elems != to_write)
    {
        f->is_empty = 0;
    }

    *nb_elems -= to_write;

    return ret;
}

s32 svr_fifo_write(SvrDynFifo* f, void* buf, s32 nb_elems)
{
    return svr_fifo_write_common(f, (u8*)buf, &nb_elems);
}

s32 svr_fifo_peek_common(SvrDynFifo* f, u8* buf, s32* nb_elems)
{
    s32 to_read = *nb_elems;
    s32 offset_r = f->offset_r;
    s32 can_read = svr_fifo_can_read(f);
    s32 ret = 0;

    if (to_read > can_read)
    {
        *nb_elems = 0;
        return -1;
    }

    if (offset_r >= f->nb_elems)
    {
        offset_r -= f->nb_elems;
    }

    while (to_read > 0)
    {
        s32 len = svr_min(f->nb_elems - offset_r, to_read);
        u8* read_ptr = f->buffer + offset_r * f->elem_size;

        memcpy(buf, read_ptr, len * f->elem_size);
        buf += len * f->elem_size;

        offset_r += len;

        if (offset_r >= f->nb_elems)
        {
            offset_r = 0;
        }

        to_read -= len;
    }

    *nb_elems -= to_read;

    return ret;
}

s32 svr_fifo_read(SvrDynFifo* f, void* buf, s32 nb_elems)
{
    s32 ret = svr_fifo_peek_common(f, (u8*)buf, &nb_elems);
    svr_fifo_drain(f, nb_elems);
    return ret;
}

void svr_fifo_drain(SvrDynFifo* f, s32 size)
{
    s32 cur_size = svr_fifo_can_read(f);

    assert(cur_size >= size);

    if (cur_size == size)
    {
        f->is_empty = 1;
    }

    if (f->offset_r >= f->nb_elems - size)
    {
        f->offset_r -= f->nb_elems - size;
    }

    else
    {
        f->offset_r += size;
    }
}

void svr_fifo_reset(SvrDynFifo* f)
{
    f->offset_r = 0;
    f->offset_w = 0;
    f->is_empty = 1;
}

void svr_fifo_free(SvrDynFifo* f)
{
    if (f->buffer)
    {
        svr_free(f->buffer);
        f->buffer = NULL;
    }

    svr_free(f);
}
