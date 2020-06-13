#include <svr/platform.hpp>
#include <svr/table.hpp>

namespace svr
{
    platform_type platform_from_name(const char* name)
    {
        table platforms = {
            table_pair{"none", PLATFORM_NONE},
            table_pair{"windows", PLATFORM_WINDOWS}
        };

        return table_map_key_or(platforms, name, PLATFORM_NONE);
    }
}
