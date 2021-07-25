#pragma once
#include "svr_common.h"

// User level semaphore, based on "Creating a semaphore from WaitOnAddress" by Raymond https://devblogs.microsoft.com/oldnewthing/20170612-00/?p=96375

struct SvrSemaphore
{
    s32 count;
    s32 max_count;
};

void svr_sem_init(SvrSemaphore* sem, s32 init_count, s32 max_count);
void svr_sem_release(SvrSemaphore* sem);
void svr_sem_wait(SvrSemaphore* sem);
