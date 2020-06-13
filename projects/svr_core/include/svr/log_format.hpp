#pragma once
#include <svr/log.hpp>
#include <svr/format.hpp>

namespace svr
{
    template <typename... Args>
    void log(const char* format, Args&&... args)
    {
        if (!log_enabled())
        {
            return;
        }

        if constexpr(sizeof...(Args) == 0)
        {
            log(format.data());
            return;
        }

        // Prefer to not allocate anything.
        // This reserves practically every message on the stack.

        fmt::memory_buffer buf;
        format_with_null(buf, format, std::forward<Args>(args)...);

        log(buf.data());
    }
}
