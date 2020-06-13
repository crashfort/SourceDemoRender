#pragma once
#include <svr/api.hpp>

namespace svr
{
    struct config_node;

    struct version_pair
    {
        int major;
        int minor;
    };

    struct version_data
    {
        version_pair core;
        version_pair game;
        version_pair game_launcher_cli;
        version_pair game_config;
    };

    SVR_API version_data version_parse(config_node* n);

    inline bool version_greater_than(version_pair v, int major, int minor)
    {
        return v.major > major && v.minor > minor;
    }
}
