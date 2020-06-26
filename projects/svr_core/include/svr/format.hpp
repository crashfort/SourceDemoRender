#pragma once
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#undef FMT_HEADER_ONLY

namespace svr
{
    // Formats to a stack reserved buffer.
    // The buffer reserves 500 bytes.
    // In case the buffer is too small, a heap allocation is made.
    template <typename... Args>
    inline void format_with_null(fmt::memory_buffer& buf, const char* format, Args&&... args)
    {
        // Reset the writing position to the start of the buffer.
        // This doesn't actually clear anything.
        buf.clear();

        auto it = fmt::format_to(buf, format, std::forward<Args>(args)...);
        *it = 0;
    }
}
