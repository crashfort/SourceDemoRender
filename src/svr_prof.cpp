#include "svr_prof.h"
#include <Windows.h>

LARGE_INTEGER prof_timer_freq;

s64 svr_prof_get_real_time()
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

#if SVR_PROF

void svr_start_prof(SvrProf& prof)
{
    prof.start = svr_prof_get_real_time();
}

void svr_end_prof(SvrProf& prof)
{
    prof.runs++;
    prof.total += (svr_prof_get_real_time() - prof.start);
}

void svr_reset_prof(SvrProf& prof)
{
    prof.runs = 0;
    prof.total = 0;
}

#endif
