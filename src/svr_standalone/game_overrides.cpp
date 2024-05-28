#include "game_priv.h"

// Convert sample format and send.
void game_prepare_and_send_sound_0(GameSndSample0* paint_buf, s32 num_samples)
{
    if (!svr_is_audio_enabled())
    {
        return;
    }

    SvrWaveSample* buf = (SvrWaveSample*)_alloca(sizeof(SvrWaveSample) * num_samples);

    for (s32 i = 0; i < num_samples; i++)
    {
        GameSndSample0* sample = &paint_buf[i];
        buf[i] = SvrWaveSample { (s16)sample->left, (s16)sample->right };
    }

    svr_give_audio(buf, num_samples);
}

// ----------------------------------------------------------------

void __cdecl game_snd_tx_stereo_override_0(void* unk, GameSndSample0* paint_buf, s32 paint_time, s32 end_time)
{
    if (!svr_movie_active())
    {
        return;
    }

    assert(game_state.snd_is_painting);

    s32 num_samples = end_time - paint_time;
    game_prepare_and_send_sound_0(paint_buf, num_samples);
}

// ----------------------------------------------------------------

void __fastcall game_snd_device_tx_samples_override_0(void* p, void* edx, u32 unused)
{
    if (!svr_movie_active())
    {
        return;
    }

    assert(game_state.snd_is_painting);
    assert(game_state.snd_num_samples);

    GameSndSample0* paint_buf = game_get_snd_paint_buffer_0();
    game_prepare_and_send_sound_0(paint_buf, game_state.snd_num_samples);
}

// ----------------------------------------------------------------

void __cdecl game_snd_paint_chans_override_0(s32 end_time, bool is_underwater)
{
    game_state.snd_listener_underwater = is_underwater;

    if (svr_movie_active() && !game_state.snd_is_painting)
    {
        return; // When movie is active we call this ourselves with the real number of samples write.
    }

    // Will call game_snd_tx_stereo_override_0 or game_snd_device_tx_samples_override_0.

    using OrgFn = decltype(game_snd_paint_chans_override_0)*;
    OrgFn org_fn = (OrgFn)game_state.snd_paint_chans_hook.original;
    org_fn(end_time, is_underwater);
}

// ----------------------------------------------------------------

void __cdecl game_start_movie_override_0(void* cmd_args)
{
    game_rec_start_movie(cmd_args);
}

// ----------------------------------------------------------------

void __cdecl game_end_movie_override_0(void* cmd_args)
{
    game_rec_end_movie();
}

// ----------------------------------------------------------------

bool __fastcall game_eng_filter_time_override_0(void* p, void* edx, float dt)
{
    bool ret = game_rec_run_frame();

    if (!ret)
    {
        using OrgFn = decltype(game_eng_filter_time_override_0)*;
        OrgFn org_fn = (OrgFn)game_state.filter_time_hook.original;
        ret = org_fn(p, edx, dt);
    }

    return ret;
}

// ----------------------------------------------------------------

void game_overrides_init()
{
    game_hook_create(&game_state.search_desc.start_movie_override, &game_state.start_movie_hook);
    game_hook_create(&game_state.search_desc.end_movie_override, &game_state.end_movie_hook);
    game_hook_create(&game_state.search_desc.filter_time_override, &game_state.filter_time_hook);

    if (game_state.search_desc.caps & GAME_CAP_HAS_AUDIO)
    {
        game_hook_create(&game_state.search_desc.snd_paint_chans_override, &game_state.snd_paint_chans_hook);
    }

    if (game_state.search_desc.caps & GAME_CAP_AUDIO_DEVICE_1)
    {
        game_hook_create(&game_state.search_desc.snd_tx_stereo_override, &game_state.snd_tx_stereo_hook);
    }

    if (game_state.search_desc.caps & GAME_CAP_AUDIO_DEVICE_2)
    {
        game_hook_create(&game_state.search_desc.snd_device_tx_samples_override, &game_state.snd_device_tx_samples_hook);
    }

    // If the game has a restriction that prevents cvars from changing when in game or demo playback.
    // This replaces the flags to compare to, so the comparison will always be false.
    s32 cvar_restrict_patch_bytes = 0x00;
    game_apply_patch(game_state.search_desc.cvar_patch_restrict_addr, &cvar_restrict_patch_bytes, sizeof(cvar_restrict_patch_bytes));
}
