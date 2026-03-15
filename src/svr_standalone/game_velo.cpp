#include "game_priv.h"

void game_velo_frame()
{
    if (!svr_is_velo_enabled())
    {
        return;
    }

    void* player = game_get_active_player();

    if (player)
    {
        SvrVec3 vel = game_get_entity_velocity(player);
        svr_give_velocity((float*)&vel);
    }
}
