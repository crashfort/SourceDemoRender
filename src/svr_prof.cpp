#include "svr_prof.h"
#include <Windows.h>

#if SVR_PROF

LARGE_INTEGER prof_timer_freq;

s64 prof_get_real_time()
{
    LARGE_INTEGER cur_time;
    QueryPerformanceCounter(&cur_time);

    s64 ret = cur_time.QuadPart * 1000000;
    ret = ret / prof_timer_freq.QuadPart;

    return ret;
}

void svr_init_prof()
{
    QueryPerformanceFrequency(&prof_timer_freq);
}

void svr_start_prof(SvrProf& prof)
{
    prof.start = prof_get_real_time();
}

void svr_end_prof(SvrProf& prof)
{
    prof.runs++;
    prof.total += (prof_get_real_time() - prof.start);
}

void svr_reset_prof(SvrProf& prof)
{
    prof.runs = 0;
    prof.total = 0;
}

#endif
