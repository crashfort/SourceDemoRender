#include "game_priv.h"

struct GameAudioSearch
{
    GameCaps caps;
    GameAudioDesc* desc;
};

GameAudioSearch GAME_AUDIO_BACKENDS[] =
{
    GameAudioSearch { GAME_CAP_AUDIO_DEVICE_1, &game_audio_v1_desc },
    GameAudioSearch { GAME_CAP_AUDIO_DEVICE_2, &game_audio_v1_desc },
};

void game_audio_init()
{
    for (s32 i = 0; i < SVR_ARRAY_SIZE(GAME_AUDIO_BACKENDS); i++)
    {
        GameAudioSearch* s = &GAME_AUDIO_BACKENDS[i];

        if (game_state.search_desc.caps & s->caps)
        {
            game_state.audio_desc = s->desc;
            break;
        }
    }

    if (game_state.audio_desc)
    {
        svr_log("Using game audio backend %s\n", game_state.audio_desc->name);
        game_state.audio_desc->init();
    }
}

void game_audio_free()
{
    if (game_state.audio_desc)
    {
        game_state.audio_desc->free();
        game_state.audio_desc = NULL;
    }
}

void game_audio_frame()
{
    if (svr_is_audio_enabled())
    {
        if (game_state.audio_desc)
        {
            game_state.audio_desc->mix_audio_for_one_frame();
        }
    }
}
