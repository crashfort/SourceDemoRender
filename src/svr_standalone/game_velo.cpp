#include "game_priv.h"

void game_velo_frame()
{
    if (!svr_is_velo_enabled())
    {
        return;
    }

    void* player = game_velo_get_active_player();

    if (player)
    {
        SvrVec3 vel = game_get_entity_velocity(player);
        svr_give_velocity((float*)&vel);
    }
}

// Return local player or spectated player (for mp games).
void* game_velo_get_active_player()
{
    // There is a bug here if you go from a demo with bots to a local game with bots,
    // where the sticky indexes are still valid so we end up not reading from the local player.
    // Not sure how to reset the spec target easily without introducing yet more patterns.

    void* player = NULL;

    s32 spec = game_get_spec_target();

    if (spec > 0)
    {
        player = game_get_player_by_index(spec);
    }

    // It's possible to be spectating someone without that player having any entity.
    if (player == NULL)
    {
        player = game_get_local_player();
    }

    return player;
}

