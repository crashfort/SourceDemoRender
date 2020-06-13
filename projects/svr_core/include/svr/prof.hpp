#pragma once
#include <stdint.h>
#include <chrono>

namespace svr
{
    struct profiler
    {
        int64_t hits = 0;
        std::chrono::high_resolution_clock::time_point start_point;
        std::chrono::microseconds total = {};
    };

    inline void prof_enter(profiler& prof)
    {
        prof.start_point = std::chrono::high_resolution_clock::now();
    }

    inline void prof_exit(profiler& prof)
    {
        prof.hits++;
        auto end_point = std::chrono::high_resolution_clock::now();
        prof.total += std::chrono::duration_cast<decltype(prof.total)>(end_point - prof.start_point);
    }

    inline uint64_t prof_avg(profiler& prof)
    {
        return prof.total.count() / prof.hits;
    }
}
