#include "game_priv.h"

void* game_get_active_player()
{
    if (game_state.search_desc.caps & GAME_CAP_LOCAL_PLAYER_DUMB)
    {
        return game_get_active_player_dumb();
    }

    if (game_state.search_desc.caps & GAME_CAP_LOCAL_PLAYER_SMART)
    {
        return game_get_active_player_smart();
    }

    return NULL;
}

// Return local player or spectated player (for mp games).
void* game_get_active_player_dumb()
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

void* game_get_active_player_smart()
{
    return game_get_spec_target_or_local_player();
}
