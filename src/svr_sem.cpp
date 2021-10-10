#include "svr_sem.h"
#include <Windows.h>
#include <assert.h>

void svr_sem_init(SvrSemaphore* sem, s32 init_count, s32 max_count)
{
    sem->count = init_count;
    sem->max_count = max_count;
}

void svr_sem_release(SvrSemaphore* sem)
{
    while (true)
    {
        s32 orig_count = sem->count;
        s32 new_count = orig_count + 1;

        // Would surpass max count.
        assert(new_count <= sem->max_count);

        LONG prev_count = InterlockedCompareExchange((volatile LONG*)&sem->count, (LONG)new_count, (LONG)orig_count);

        if (prev_count == (LONG)orig_count)
        {
            WakeByAddressSingle(&sem->count);
            return;
        }
    }
}

void svr_sem_wait(SvrSemaphore* sem)
{
    while (true)
    {
        s32 orig_count = sem->count;

        while (orig_count == 0)
        {
            WaitOnAddress(&sem->count, &orig_count, sizeof(s32), INFINITE);
            orig_count = sem->count;
        }

        LONG prev_count = InterlockedCompareExchange((volatile LONG*)&sem->count, (LONG)orig_count - 1, (LONG)orig_count);

        if (prev_count == (LONG)orig_count)
        {
            return;
        }
    }
}
