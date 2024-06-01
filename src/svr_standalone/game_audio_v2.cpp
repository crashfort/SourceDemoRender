#include "game_priv.h"

// The audio backend need times to be aligned to 4 sample boundaries.
// The skipped samples are remembered for the next iteration.
s64 game_audio_v2_align_sample_time(s64 value)
{
    return value & ~3;
}

void game_audio_v2_init()
{
}

void game_audio_v2_free()
{
}

void game_audio_v2_mix_audio_for_one_frame()
{
    // Figure out how many samples we need to process for this frame.

    s64 paint_time = game_get_snd_paint_time_1();

    float time_ahead_to_mix = 1.0f / (float)game_state.rec_game_rate;
    float num_frac_samples_to_mix = (time_ahead_to_mix * game_state.search_desc.snd_sample_rate) + game_state.snd_lost_mix_time;

    s64 num_samples_to_mix = (s64)num_frac_samples_to_mix;
    game_state.snd_lost_mix_time = num_frac_samples_to_mix - (float)num_samples_to_mix;

    s64 raw_end_time = paint_time + num_samples_to_mix + game_state.snd_skipped_samples;
    s64 aligned_end_time = game_audio_v2_align_sample_time(raw_end_time);

    s64 num_samples = aligned_end_time - paint_time;

    game_state.snd_skipped_samples = raw_end_time - aligned_end_time;
    game_state.snd_num_samples = num_samples;

    if (num_samples > 0)
    {
        game_state.snd_is_painting = true;
        game_snd_paint_chans_override_1(aligned_end_time, game_state.snd_listener_underwater);
        game_state.snd_is_painting = false;
    }
}

GameAudioDesc game_audio_v2_desc =
{
    .name = "AudioV2",
    .init = game_audio_v2_init,
    .free = game_audio_v2_free,
    .mix_audio_for_one_frame = game_audio_v2_mix_audio_for_one_frame,
};
