#include "game_version.hpp"

#include <svr/config.hpp>
#include <svr/defer.hpp>
#include <svr/str.hpp>
#include <svr/log_format.hpp>

svr::version_data game_version;

bool game_version_init(const char* resource_path)
{
    using namespace svr;

    str_builder builder;
    builder.append(resource_path);
    builder.append("data/version.json");

    auto cfg = config_open_json(builder.buf);

    if (cfg == nullptr)
    {
        log("Could not open version file\n");
        return false;
    }

    defer {
        config_destroy(cfg);
    };

    game_version = version_parse(config_root(cfg));
    return true;
}
