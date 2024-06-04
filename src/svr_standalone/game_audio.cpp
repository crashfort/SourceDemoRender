#include "game_priv.h"

struct GameAudioSearch
{
    GameCaps caps;
    GameAudioDesc* desc;
};

GameAudioSearch GAME_AUDIO_BACKENDS[] =
{
    GameAudioSearch { GAME_CAP_AUDIO_DEVICE_1, &game_audio_v1_desc },
    GameAudioSearch { GAME_CAP_AUDIO_DEVICE_1_5, &game_audio_v1_desc },
    GameAudioSearch { GAME_CAP_AUDIO_DEVICE_2, &game_audio_v2_desc },
};

void game_audio_init()
{
    GameAudioDesc* best_desc = NULL;
    s32 best_match = 0;

    for (s32 i = 0; i < SVR_ARRAY_SIZE(GAME_AUDIO_BACKENDS); i++)
    {
        GameAudioSearch* s = &GAME_AUDIO_BACKENDS[i];

        s32 num_match = svr_count_set_bits(game_state.search_desc.caps & s->caps);

        if (num_match > best_match)
        {
            best_desc = s->desc;
            best_match = num_match;
        }
    }

    game_state.audio_desc = best_desc;

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
            // Figure out how many samples we need to process for this frame.

            float time_ahead_to_mix = 1.0f / (float)game_state.rec_game_rate;
            float num_frac_samples_to_mix = (time_ahead_to_mix * game_state.search_desc.snd_sample_rate) + game_state.snd_lost_mix_time;

            s32 num_samples_to_mix = (s32)num_frac_samples_to_mix;
            game_state.snd_lost_mix_time = num_frac_samples_to_mix - (float)num_samples_to_mix;

            game_state.audio_desc->mix_audio_for_one_frame(num_samples_to_mix);
        }
    }
}
