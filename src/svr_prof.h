#pragma once
#include "svr_common.h"

// Disable for releases.
#define SVR_PROF 1

struct SvrProf
{
    s64 start;
    s64 runs;
    s64 total;
};

#if SVR_PROF

void svr_init_prof();

void svr_start_prof(SvrProf& prof);
void svr_end_prof(SvrProf& prof);
void svr_reset_prof(SvrProf& prof);

#else

#define svr_init_prof()

#define svr_start_prof()
#define svr_end_prof()
#define svr_reset_prof()

#endif
