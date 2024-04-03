#pragma once
#include "svr_common.h"

// Fast dynamic FIFO queue.
// Suitable for byte streams and structures.
// Based on https://ffmpeg.org/doxygen/trunk/libavutil_2fifo_8c_source.html by the FFmpeg developers!
// This cannot be a template because it makes MSVC produce really slow code (25x slower). Could not figure out why.

struct SvrDynFifo;

// Allocates a new FIFO.
SvrDynFifo* svr_fifo_alloc(s32 nb_elems, s32 elem_size);

// Returns how many items you can read right now.
s32 svr_fifo_can_read(SvrDynFifo* f);

// Pushes items at the back.
s32 svr_fifo_write(SvrDynFifo* f, void* buf, s32 nb_elems);

// Pops items from the front.
s32 svr_fifo_read(SvrDynFifo* f, void* buf, s32 nb_elems);

// Pops items from the front without reading.
void svr_fifo_drain(SvrDynFifo* f, s32 size);

// Clears the FIFO.
void svr_fifo_reset(SvrDynFifo* f);

// Frees the FIFO.
void svr_fifo_free(SvrDynFifo* f);
