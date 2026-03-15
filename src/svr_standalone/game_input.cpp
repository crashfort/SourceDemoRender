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
