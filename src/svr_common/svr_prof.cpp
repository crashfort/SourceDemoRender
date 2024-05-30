#include "svr_prof.h"
#include <Windows.h>
#include <assert.h>

LARGE_INTEGER prof_timer_freq;

s64 svr_prof_get_real_time()
{
    assert(prof_timer_freq.QuadPart != 0);

    LARGE_INTEGER cur_time;
    QueryPerformanceCounter(&cur_time);

    s64 ret = cur_time.QuadPart * 1000000;
    ret = ret / prof_timer_freq.QuadPart;

    return ret;
}

void svr_prof_init()
{
    QueryPerformanceFrequency(&prof_timer_freq);
}

void svr_prof_start(SvrProf* prof)
{
    prof->start = svr_prof_get_real_time();
}

void svr_prof_end(SvrProf* prof)
{
    prof->runs++;
    prof->total += (svr_prof_get_real_time() - prof->start);
}

void svr_prof_reset(SvrProf* prof)
{
    prof->runs = 0;
    prof->total = 0;
}
