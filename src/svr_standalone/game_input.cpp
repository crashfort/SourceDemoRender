#include "game_priv.h"

void game_input_frame()
{
    if (!svr_is_input_enabled())
    {
        return;
    }

    void* player = game_get_active_player();

    if (player)
    {
        u32 bits = game_get_player_buttons(player);

        // Use this as detection for SVR STV addon.
        // I don't know how else to detect it. This is important because the SVR STV addon will store the velo
        // in the demo when it normally is not. This confuses the client code which already tries to estimate the velo.
        // When both are active at the same time it causes jitter.
        // The reason this has to be done dynamically is because not all demos or environments will be using the addon.
        if (bits != 0)
        {
            if (!game_state.has_found_svr_stv_addon_demo)
            {
                game_state.has_found_svr_stv_addon_demo = true;
                game_enable_local_velo_estimation(false);
            }
        }

        SvrButtons buttons = {};
        buttons.in_attack = (bits & game_state.search_desc.in_attack) != 0;
        buttons.in_jump = (bits & game_state.search_desc.in_jump) != 0;
        buttons.in_duck = (bits & game_state.search_desc.in_duck) != 0;
        buttons.in_forward = (bits & game_state.search_desc.in_forward) != 0;
        buttons.in_back = (bits & game_state.search_desc.in_back) != 0;
        buttons.in_yaw_left = (bits & game_state.search_desc.in_yaw_left) != 0;
        buttons.in_yaw_right = (bits & game_state.search_desc.in_yaw_right) != 0;
        buttons.in_move_left = (bits & game_state.search_desc.in_move_left) != 0;
        buttons.in_move_right = (bits & game_state.search_desc.in_move_right) != 0;
        buttons.in_walk = (bits & game_state.search_desc.in_walk) != 0;

        svr_give_buttons(buttons);
    }
}
