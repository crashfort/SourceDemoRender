#include "proc_priv.h"
#include "proc_state.h"

bool ProcState::init(const char* in_resource_path, ID3D11Device* in_d3d11_device)
{
    bool ret = false;

    SVR_COPY_STRING(in_resource_path, svr_resource_path);

    if (!vid_init(in_d3d11_device))
    {
        goto rfail;
    }

    if (!velo_init())
    {
        goto rfail;
    }

    if (!input_init())
    {
        goto rfail;
    }

    if (!mosample_init())
    {
        goto rfail;
    }

    if (!encoder_init())
    {
        goto rfail;
    }

    if (!studio_init())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    free_static();

rexit:

    return ret;
}

void ProcState::new_video_frame()
{
    // If we are using mosample, we will have to accumulate enough frames before we can start sending.
    // Mosample will internally send the frames when they are ready.
    if (movie_profile.mosample_enabled)
    {
        mosample_new_video_frame();
    }

    // No mosample, just send the frame over directly.
    else
    {
        vid_d3d11_context->CopyResource(encoder_share_tex, svr_game_texture.tex);
        process_finished_shared_tex();
    }
}

void ProcState::new_audio_samples(SvrWaveSample* samples, s32 num_samples)
{
    encoder_send_audio_samples(samples, num_samples);
}

bool ProcState::is_velo_enabled()
{
    return movie_profile.velo_enabled;
}

bool ProcState::is_input_enabled()
{
    return movie_profile.input_enabled;
}

bool ProcState::is_audio_enabled()
{
    return movie_profile.audio_enabled;
}

// Call this when you have written everything you need to encoder_share_tex.
void ProcState::process_finished_shared_tex()
{
    // Now is the time to draw the additions we have any.

    if (movie_profile.velo_enabled)
    {
        velo_draw();
    }

    if (movie_profile.input_enabled)
    {
        input_draw();
    }

    encoder_send_shared_tex();

    movie_lagcomp_queued_time += movie_lagcomp_frame_time;

    if (movie_lagcomp_queued_time >= movie_lagcomp_interp)
    {
        movie_lagcomp_queued_time = movie_lagcomp_interp;
    }
}

bool ProcState::start(const char* dest_file, const char* profile, ProcGameTexture* game_texture, SvrAudioParams* audio_params)
{
    bool ret = false;

    svr_game_texture = *game_texture;
    svr_audio_params = *audio_params;

    // Build output video path.

    SVR_SNPRINTF(movie_path, "%s\\movies\\", svr_resource_path);
    CreateDirectoryA(movie_path, NULL);
    SVR_SNPRINTF(movie_path, "%s\\movies\\%s", svr_resource_path, dest_file);

    movie_setup_params();

    // Must load the profiles first!
    // The default profile is the base profile, and other profiles can override individual options.
    movie_setup_default_profile();

    if (!movie_load_profile("default"))
    {
        goto rfail;
    }

    if (profile && profile[0])
    {
        if (!movie_load_profile(profile))
        {
            goto rfail;
        }
    }

    if (!vid_start())
    {
        goto rfail;
    }

    if (!mosample_start())
    {
        goto rfail;
    }

    if (!velo_start())
    {
        goto rfail;
    }

    if (!input_start())
    {
        goto rfail;
    }

    if (!encoder_start())
    {
        goto rfail;
    }

    if (!studio_start())
    {
        goto rfail;
    }

    setup_lag_compensation();

    ret = true;
    goto rexit;

rfail:
    free_dynamic();

rexit:
    return ret;
}

void ProcState::end()
{
    encoder_end();
    mosample_end();
    velo_end();
    input_end();
    vid_end();

    free_dynamic();
}

void ProcState::free_static()
{
    studio_free_static();
    encoder_free_static();
    mosample_free_static();
    velo_free_static();
    input_free_static();
    vid_free_static();
}

void ProcState::free_dynamic()
{
    studio_free_dynamic();
    encoder_free_dynamic();
    mosample_free_dynamic();
    velo_free_dynamic();
    input_free_dynamic();
    vid_free_dynamic();

    svr_game_texture = {};
}

s32 ProcState::get_game_rate()
{
    if (movie_profile.mosample_enabled)
    {
        return movie_profile.video_fps * movie_profile.mosample_mult;
    }

    return movie_profile.video_fps;
}

void ProcState::setup_lag_compensation()
{
    movie_lagcomp_frame_time = 1.0f / (float)movie_profile.video_fps;
    movie_lagcomp_queued_time = 0.0f;
    movie_lagcomp_interp = movie_profile.lagcomp_override;
}
