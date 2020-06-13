#include <svr/game.hpp>
#include <svr/log_format.hpp>
#include <svr/game_config.hpp>
#include <svr/table.hpp>

bool arch_code_000_source_1_win(svr::game_config_game* game, const char* resource_path);

static svr::table ARCHS = {
    svr::table_pair{"code-000-source-1-win", arch_code_000_source_1_win}
};

bool game_external_init(svr::game_config_game* game, const char* resource_path)
{
    using namespace svr;

    auto func = table_map_key_or(ARCHS, game_config_game_arch(game), nullptr);

    if (func)
    {
        return func(game, resource_path);
    }

    log("Could not find any arch for game '{}'\n", game_config_game_id(game));
    return false;
}
