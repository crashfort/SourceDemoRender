#include <svr/platform.hpp>

namespace svr
{
    platform_type platform_current()
    {
        return PLATFORM_WINDOWS;
    }

    const char* platform_exe()
    {
        return ".exe";
    }
}
