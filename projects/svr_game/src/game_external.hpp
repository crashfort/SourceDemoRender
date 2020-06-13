#pragma once

namespace svr
{
    struct game_config_game;
}

bool game_external_init(svr::game_config_game* game, const char* resource_path);
