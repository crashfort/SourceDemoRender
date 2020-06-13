#include <svr/game.hpp>
#include <svr/log_format.hpp>
#include <svr/game_config.hpp>

bool arch_code_000_source_1_win(svr::game_config_game* game, const char* resource_path);

struct arch
{
    const char* name;
    bool(*func)(svr::game_config_game* game, const char* resource_path);
};

static arch archs[] = {
    arch {"code-000-source-1-win", arch_code_000_source_1_win}
};

bool game_external_init(svr::game_config_game* game, const char* resource_path)
{
    for (auto entry : archs)
    {
        if (strcmp(entry.name, game_config_game_arch(game)) == 0)
        {
            return entry.func(game, resource_path);
        }
    }

    return false;
}
