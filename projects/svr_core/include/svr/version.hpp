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
        version_pair app;
    };

    SVR_API version_data version_parse(config_node* n);

    inline bool version_greater_than(version_pair v, int major, int minor)
    {
        auto a = v.major * 1000 + v.minor;
        auto b = major * 1000 + minor;

        return a > b;
    }

    inline bool version_greater_than(version_pair a, version_pair b)
    {
        return version_greater_than(a, b.major, b.minor);
    }
}
