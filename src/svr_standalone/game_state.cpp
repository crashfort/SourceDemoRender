#include "game_priv.h"

void GameState::start_movie(void* cmd_args)
{
    if (svr_movie_active())
    {
        game_console_msg("Movie already started\n");
        return;
    }

    // First argument is always startmovie.

    const char* args = NULL;
    game_desc.get_cmd_args_proxy.proxy(&game_desc.get_cmd_args_proxy, cmd_args, &args);

    const char* value_args = svr_advance_until_whitespace(args);
    value_args = svr_advance_until_after_whitespace(value_args);

    if (*value_args == 0)
    {
        show_start_movie_usage();
        return;
    }

    char movie_name[MAX_PATH];
    movie_name[0] = 0;

    char profile_name[256];
    profile_name[0] = 0;

    value_args = svr_extract_string(value_args, movie_name, SVR_ARRAY_SIZE(movie_name));

    if (movie_name[0] == 0)
    {
        show_start_movie_usage();
        return;
    }

    SvrDynArray<SvrIniKeyValue*> inputs = {};
    svr_ini_parse_command_input(value_args, &inputs);

    const char* opt_profile = svr_ini_find_command_value(&inputs, "profile");
    const char* opt_timeout = svr_ini_find_command_value(&inputs, "timeout");
    const char* opt_autostop = svr_ini_find_command_value(&inputs, "autostop");
    const char* opt_no_wind_upd = svr_ini_find_command_value(&inputs, "nowindupd");

    if (opt_profile)
    {
        SVR_COPY_STRING(opt_profile, profile_name);
    }

    if (opt_timeout)
    {
        rec_timeout = atoi(opt_timeout);
    }

    if (opt_autostop)
    {
        rec_enable_autostop = atoi(opt_autostop);
    }

    if (opt_no_wind_upd)
    {
        rec_disable_window_update = atoi(opt_no_wind_upd);
    }

    svr_ini_free_kvs(&inputs);

    // Will point to the end if no extension was provided.
    const char* movie_ext = PathFindExtensionA(movie_name);

    // Only allowed containers that have sufficient encoder support.
    bool has_valid_ext = !strcmpi(movie_ext, ".mp4") || !strcmpi(movie_ext, ".mkv") || !strcmpi(movie_ext, ".mov");

    if (!has_valid_ext)
    {
        game_console_msg("File extension is wrong or missing. You may choose between MP4, MKV, MOV\n");
        game_console_msg("\n");
        game_console_msg("Example:\n");
        game_console_msg("\n");
        game_console_msg("    startmovie a.mov\n");
        game_console_msg("\n");
        game_console_msg("For more information see https://github.com/crashfort/SourceDemoRender\n");
        return;
    }

    // Some commands must be set before svr_start (such as mat_queue_mode 0, due to the backbuffer ordering of the GetRenderTarget call).
    // This file must always be run! Movie cannot be started otherwise!
    if (!run_cfg("svr_movie_start.cfg"))
    {
        game_log("Required cfg svr_start_movie.cfg could not be run\n");
        goto rfail;
    }

    run_user_cfgs_for_event("start");

    // The game backbuffer is the first index.
    IDirect3DSurface9* bb_surf = NULL;
    game_desc.d3d9ex_device_ptr->GetRenderTarget(0, &bb_surf);

    SvrStartMovieData startmovie_data;
    startmovie_data.game_tex_view = bb_surf;
    startmovie_data.audio_params.audio_channels = GAME_SND_CHANS;
    startmovie_data.audio_params.audio_hz = GAME_SND_RATE;
    startmovie_data.audio_params.audio_bits = GAME_SND_BITS;

    if (!svr_start(movie_name, profile_name, &startmovie_data))
    {
        // Reverse above changes if something went wrong.
        run_cfg("svr_movie_end.cfg");
        run_user_cfgs_for_event("end");
        goto rfail;
    }

    // Ensure the game runs at a fixed rate.

    rec_game_rate = svr_get_game_rate();

    engine_client_command(svr_va("host_framerate %d\n", rec_game_rate));

    // Allow recording the next frame.
    rec_state = GAME_REC_WAITING;

    rec_num_frames = 0;
    rec_first_frame_time = 0;

    snd_skipped_samples = 0;
    snd_lost_mix_time = 0.0f;
    snd_num_samples = 0;

    rec_timeout = 0;
    rec_enable_autostop = true;
    rec_disable_window_update = false;

    game_log("Starting movie to %s\n", movie_name);

    goto rexit;

rfail:

rexit:
    svr_maybe_release(&bb_surf);
}

void GameState::end_movie()
{
}

bool GameState::run_frame()
{
    if (game_desc.has_autostop_required)
    {
        rec_update_autostop();
    }

    if (rec_state == GAME_REC_POSSIBLE && svr_movie_active())
    {
        do_recording_frame();
        return true;
    }

    return false;
}

void GameState::send_sound(SvrWaveSample* samples, s32 num_samples)
{
}

void GameState::do_recording_frame()
{
    if (rec_num_frames == 0)
    {
        rec_first_frame_time = svr_prof_get_real_time();
    }

    mix_audio_for_one_frame();

    if (svr_is_velo_enabled())
    {
        velo_give();
    }

    svr_frame();

    rec_num_frames++;

    wind_update();
    rec_update_timeout();
}

bool GameState::init(GameDesc* desc, SvrGameInitData* init_data)
{
    bool ret = false;

    game_desc = *desc;
    svr_path = init_data->svr_path;

    svr_log("Has core module: %d\n", game_desc.has_core_required);
    svr_log("Has velo module: %d\n", game_desc.has_velo_required);
    svr_log("Has audio module: %d\n", game_desc.has_audio_required);
    svr_log("Has audio v1 module: %d\n", game_desc.has_audio_v1_required);
    svr_log("Has audio v2 module: %d\n", game_desc.has_audio_v2_required);
    svr_log("Has autostop module: %d\n", game_desc.has_autostop_required);

    if (game_desc.has_velo_required)
    {
        if (!init_core())
        {
            goto rfail;
        }

        if (game_desc.has_velo_required)
        {
            if (!init_velo())
            {
                goto rfail;
            }
        }

        if (game_desc.has_audio_required)
        {
            if (!init_audio())
            {
                goto rfail;
            }

            if (game_desc.has_audio_v1_required)
            {
                if (!init_audio_v1())
                {
                    goto rfail;
                }
            }

            if (game_desc.has_audio_v2_required)
            {
                if (!init_audio_v2())
                {
                    goto rfail;
                }
            }
        }

        if (game_desc.has_autostop_required)
        {
            if (!init_autostop())
            {
                goto rfail;
            }
        }
    }

    ret = true;
    goto rexit;

rfail:
rexit:
    return ret;
}

bool GameState::init_core()
{
    hook_init();

    hook_function(&game_desc.start_movie_override, &start_movie_hook);
    hook_function(&game_desc.end_movie_override, &end_movie_hook);
    hook_function(&game_desc.filter_time_override, &eng_filter_time_hook);

    rec_init();

    return true;
}

bool GameState::init_velo()
{
    return true;
}

bool GameState::init_audio()
{
    hook_function(&game_desc.snd_paint_chans_override, &snd_paint_chans_hook);

    return true;
}

bool GameState::init_audio_v1()
{
    hook_function(&game_desc.snd_tx_stereo_override, &snd_tx_stereo_hook);

    return true;
}

bool GameState::init_audio_v2()
{
    hook_function(&game_desc.snd_device_tx_samples_override, &snd_device_tx_samples_hook);

    return true;
}

bool GameState::init_autostop()
{
    return true;
}

bool GameState::run_cfg(const char* name)
{
    bool ret = false;

    char full_cfg_path[MAX_PATH];
    SVR_SNPRINTF("%s\\data\\cfg\\%s", svr_path, name);

    char* mem = svr_read_file_as_string(full_cfg_path, SVR_READ_FILE_FLAGS_NEW_LINE);

    // Commands must end with a newline, also need to terminate.

    svr_log("Running cfg %s\n", name);

    // The file can be executed as is. The game takes care of splitting by newline.
    // We don't monitor what is inside the cfg, it's up to the user.
    engine_client_command(mem);

    ret = true;
    goto rexit;

rfail:
rexit:
    return ret;
}

// Run all user cfgs for a given event (such as movie start or movie end).
void GameState::run_user_cfgs_for_event(const char* name)
{
    run_cfg(svr_va("svr_movie_%s_user.cfg", name));
}

void GameState::engine_client_command(const char* cmd)
{
    assert(game_desc.has_core_required);
    game_desc.engine_client_command_proxy.proxy(&game_desc.engine_client_command_proxy, (void*)cmd, NULL);
}

SvrVec3 GameState::get_entity_velocity(void* entity)
{
    assert(game_desc.has_velo_required);

    SvrVec3 ret = {};
    game_desc.get_entity_velocity_proxy.proxy(&game_desc.get_entity_velocity_proxy, entity, &ret);
    return ret;
}

void* GameState::get_player_by_index(s32 idx)
{
    assert(game_desc.has_velo_required);

    void* ret = NULL;
    game_desc.get_player_by_index_proxy.proxy(&game_desc.get_player_by_index_proxy, &idx, &ret);
    return ret;
}

s32 GameState::get_spec_target()
{
    assert(game_desc.has_velo_required);

    s32 ret = 0;
    game_desc.get_spec_target_proxy.proxy(&game_desc.get_spec_target_proxy, NULL, &ret);
    return ret;
}

const char* GameState::get_cmd_args(void* ptr)
{
    assert(game_desc.has_core_required);

    const char* ret = NULL;
    game_desc.get_cmd_args_proxy.proxy(&game_desc.get_cmd_args_proxy, ptr, &ret);
    return ret;
}
