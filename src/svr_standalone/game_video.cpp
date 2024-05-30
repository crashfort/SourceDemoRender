#include "game_priv.h"

struct GameVideoSearch
{
    GameCaps caps;
    GameVideoDesc* desc;
};

GameVideoSearch GAME_VIDEO_BACKENDS[] =
{
    GameVideoSearch { GAME_CAP_D3D9EX_VIDEO, &game_d3d9ex_desc }
};

// Find the right video backend from the search description.
void game_video_init()
{
    for (s32 i = 0; i < SVR_ARRAY_SIZE(GAME_VIDEO_BACKENDS); i++)
    {
        GameVideoSearch* s = &GAME_VIDEO_BACKENDS[i];

        if (game_state.search_desc.caps & s->caps)
        {
            game_state.video_desc = s->desc;
            break;
        }
    }

    if (game_state.video_desc == NULL)
    {
        game_init_error("No video backend available.");
    }

    svr_log("Using game video backend %s\n", game_state.video_desc->name);

    game_state.video_desc->init();
}

void game_video_free()
{
    if (game_state.video_desc)
    {
        game_state.video_desc->free();
        game_state.video_desc = NULL;
    }
}
