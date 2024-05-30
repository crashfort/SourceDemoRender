#pragma once
#include "svr_common.h"

struct SvrProf
{
    s64 start;
    s64 runs;
    s64 total;
};

void svr_prof_init();
s64 svr_prof_get_real_time(); // Returns microseconds.

void svr_prof_start(SvrProf* prof);
void svr_prof_end(SvrProf* prof);
void svr_prof_reset(SvrProf* prof);
