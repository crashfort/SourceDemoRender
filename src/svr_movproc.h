#pragma once
#include "svr_common.h"

const s64 SVR_MOVPROC_QUEUED_FRAMES = 64;

// For game process:
void svr_create_movproc(s64 one_frame_size);

// Retrieve a buffer, write to it and then push it.
// May block if the write process is using all the buffers at the moment.

u8* svr_get_free_movproc_buf();
void svr_push_movproc_buf(u8* mem);

// For write process:
void svr_open_movproc();

u8* svr_get_used_movproc_buf();
void svr_pull_movproc_buf(u8* mem);

// For both processes:
void svr_close_movproc();
