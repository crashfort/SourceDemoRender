#pragma once
#include <svr/api.hpp>

namespace svr
{
    enum platform_type
    {
        PLATFORM_NONE,
        PLATFORM_WINDOWS,
    };

    // Returns the current plaform.
    SVR_API platform_type platform_current();

    // Returns a platform type from string.
    SVR_API platform_type platform_from_name(const char* name);
}
