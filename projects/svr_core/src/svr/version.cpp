#include <svr/version.hpp>
#include <svr/config.hpp>
#include <svr/defer.hpp>

static svr::version_pair extract_version(svr::config_node* n)
{
    using namespace svr;

    version_pair ret;
    ret.major = config_view_int64_or(config_get_array_element(n, 0), 0);
    ret.minor = config_view_int64_or(config_get_array_element(n, 1), 0);
    return ret;
};

namespace svr
{
    version_data version_parse(config_node* n)
    {
        version_data ret;
        ret.core = extract_version(config_find(n, "svr-core"));
        ret.game = extract_version(config_find(n, "svr-game"));
        ret.game_launcher_cli = extract_version(config_find(n, "svr-game-launcher-cli"));
        ret.game_config = extract_version(config_find(n, "game-config"));

        return ret;
    }
}
