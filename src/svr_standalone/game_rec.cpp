#include "game_priv.h"

void game_rec_init()
{
}

void game_rec_update_timeout()
{
    if (game_state.rec_timeout == 0)
    {
        return; // Should not stop automatically.
    }

    s64 end_frame = game_state.rec_timeout * game_state.rec_game_rate;

    // No more frames should be processed.
    if (game_state.rec_num_frames >= end_frame)
    {
        game_rec_end_movie();
    }
}

void game_rec_update_recording_state()
{
    if (!(game_state.search_desc.caps & GAME_CAP_HAS_AUTOSTOP))
    {
        return;
    }

    s32 state = game_get_signon_state();

    if (state == game_state.search_desc.signon_state_none)
    {
        // Autostop.
        if (game_state.rec_state == GAME_REC_POSSIBLE)
        {
            if (game_state.rec_enable_autostop)
            {
                game_state.rec_state = GAME_REC_STOPPED;
            }

            else
            {
                // Wait until we connect again.
                game_state.rec_state = GAME_REC_WAITING;
            }
        }
    }

    else if (state == game_state.search_desc.signon_state_full)
    {
        // Autostart.
        if (game_state.rec_state == GAME_REC_WAITING)
        {
            game_state.rec_state = GAME_REC_POSSIBLE;
        }
    }
}

void game_rec_update_autostop()
{
    // If we disconnected the previous frame, stop recording this frame.
    if (game_state.rec_state == GAME_REC_STOPPED && svr_movie_active())
    {
        game_rec_end_movie();
    }
}

void game_rec_show_start_movie_usage()
{
    svr_console_msg("Usage: startmovie <name> (<optional parameters>)\n");
    svr_console_msg("Starts to record a movie with an optional parameters.\n");
    svr_console_msg("\n");
    svr_console_msg("Optional parameters are written in the following example format:\n");
    svr_console_msg("\n");
    svr_console_msg("    startmovie a.mov timeout=40 profile=my_profile\n");
    svr_console_msg("\n");
    svr_console_msg("The order does not matter for the optional parameters, and you can omit the ones you do not need.\n");
    svr_console_msg("The parameters are for features that are per render, and not persistent like the profile.\n");
    svr_console_msg("\n");
    svr_console_msg("Optional parameters are:\n");
    svr_console_msg("\n");
    svr_console_msg("    timeout=<seconds>\n");
    svr_console_msg("    Automatically stop rendering after the elapsed video time passes.\n");
    svr_console_msg("    This will add a progress bar to the task bar icon. By default, there is no timeout.\n");
    svr_console_msg("\n");
    svr_console_msg("    profile=<string>\n");
    svr_console_msg("    Override which rendering profile to use.\n");
    svr_console_msg("    If omitted, the default profile is used.\n");
    svr_console_msg("\n");
    svr_console_msg("    autostop=<value>\n");
    svr_console_msg("    Automatically stop the movie on demo disconnect. This can be 0 or 1. Default is 1.\n");
    svr_console_msg("    This is used to determine what happens when a demo ends, when you get kicked back to the main menu.\n");
    svr_console_msg("\n");
    svr_console_msg("    nowindupd=<value>\n");
    svr_console_msg("    Disable window presentation. This can be 0 or 1. Default is 0.\n");
    svr_console_msg("    For some systems this may improve performance, however you will not be able to see anything.\n");
    svr_console_msg("\n");
    svr_console_msg("For more information see https://github.com/crashfort/SourceDemoRender\n");
}

void game_rec_start_movie(void* cmd_args)
{
    if (svr_movie_active())
    {
        svr_console_msg("Movie already started\n");
        return;
    }

    // First argument is always startmovie.

    const char* args = game_get_cmd_args(cmd_args);

    // It's possible for the engine to incorrectly parse the command line in the console, adding extra whitespace at the start in some cases.
    args = svr_advance_until_after_whitespace(args);

    if (*args == 0)
    {
        svr_console_msg("Internal engine command parsing error\n");
        return;
    }

    const char* value_args = svr_advance_until_whitespace(args);
    value_args = svr_advance_until_after_whitespace(value_args);

    if (*value_args == 0)
    {
        game_rec_show_start_movie_usage();
        return;
    }

    char movie_name[MAX_PATH];
    movie_name[0] = 0;

    value_args = svr_extract_string(value_args, movie_name, SVR_ARRAY_SIZE(movie_name));

    if (movie_name[0] == 0)
    {
        game_rec_show_start_movie_usage();
        return;
    }

    // Defaults for start args.
    game_state.rec_timeout = 0;
    game_state.rec_enable_autostop = true;
    game_state.rec_disable_window_update = false;

    char profile_name[256];
    profile_name[0] = 0;

    // Read start args.

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
        game_state.rec_timeout = atoi(opt_timeout);
    }

    if (opt_autostop)
    {
        game_state.rec_enable_autostop = atoi(opt_autostop);
    }

    if (opt_no_wind_upd)
    {
        game_state.rec_disable_window_update = atoi(opt_no_wind_upd);
    }

    svr_ini_free_kvs(&inputs);

    // Will point to the end if no extension was provided.
    const char* movie_ext = PathFindExtensionA(movie_name);

    // Only allowed containers that have sufficient encoder support.
    // Though DNxHR can only be used with MOV, we cannot check the content of the profile here.
    bool valid_exts[] =
    {
        !strcmpi(movie_ext, ".mp4"),
        !strcmpi(movie_ext, ".mkv"),
        !strcmpi(movie_ext, ".mov"),
    };

    if (!svr_check_one_true(valid_exts, SVR_ARRAY_SIZE(valid_exts)))
    {
        svr_console_msg("File extension is wrong or missing. You may choose between MP4, MKV, MOV\n");
        svr_console_msg("\n");
        svr_console_msg("Example:\n");
        svr_console_msg("\n");
        svr_console_msg("    startmovie a.mov\n");
        svr_console_msg("\n");
        svr_console_msg("For more information see https://github.com/crashfort/SourceDemoRender\n");
        return;
    }

    // These files must exist in order to set the right values.

    bool required_cfgs[] =
    {
        game_has_cfg("svr_movie_start.cfg"),
        game_has_cfg("svr_movie_end.cfg"),
    };

    if (!svr_check_all_true(required_cfgs, SVR_ARRAY_SIZE(required_cfgs)))
    {
        svr_console_msg_and_log("Required files svr_start_movie.cfg and svr_movie_end.cfg could not be found\n");
        goto rfail;
    }

    // Some commands must be set before svr_start (such as mat_queue_mode 0, due to the backbuffer ordering of the GetRenderTarget call).
    // This file must always be run! Movie cannot be started otherwise!
    game_run_cfgs_for_event("start");

    SvrStartMovieData startmovie_data;
    startmovie_data.game_tex_view = game_state.video_desc->get_game_texture();
    startmovie_data.audio_params.audio_channels = game_state.search_desc.snd_num_channels;
    startmovie_data.audio_params.audio_hz = game_state.search_desc.snd_sample_rate;
    startmovie_data.audio_params.audio_bits = game_state.search_desc.snd_bit_depth;

    if (!svr_start(movie_name, profile_name, &startmovie_data))
    {
        // Reverse above changes if something went wrong.
        game_run_cfgs_for_event("end");
        goto rfail;
    }

    // Ensure the game runs at a fixed rate.

    game_state.rec_game_rate = svr_get_game_rate();

    game_engine_client_command(svr_va("host_framerate %d\n", game_state.rec_game_rate));

    // Allow recording the next frame.
    game_state.rec_state = GAME_REC_WAITING;

    // Reset recording state.

    game_state.rec_num_frames = 0;
    game_state.rec_start_time = svr_prof_get_real_time();

    game_state.snd_skipped_samples = 0;
    game_state.snd_lost_mix_time = 0.0f;
    game_state.snd_num_samples = 0;

    svr_console_msg_and_log("Starting movie to %s\n", movie_name);

    goto rexit;

rfail:;
rexit:;
}

void game_rec_end_movie()
{
    if (!svr_movie_active())
    {
        svr_console_msg("Movie not started\n");
        return;
    }

    game_state.rec_state = GAME_REC_STOPPED;

    s64 now = svr_prof_get_real_time();

    float time_taken = 0.0f;
    float fps = 0.0f;

    if (game_state.rec_num_frames > 0)
    {
        time_taken = (now - game_state.rec_start_time) / 1000000.0f;
        fps = (float)game_state.rec_num_frames / time_taken;
    }

    svr_console_msg_and_log("Ending movie after %0.2f seconds (%lld frames, %0.2f fps)\n", time_taken, game_state.rec_num_frames, fps);

    svr_stop();

    game_run_cfgs_for_event("end");

    game_wind_reset();
}

bool game_rec_run_frame()
{
    game_rec_update_recording_state();
    game_rec_update_autostop();

    if (game_state.rec_state == GAME_REC_POSSIBLE && svr_movie_active())
    {
        game_rec_do_record_frame();
        return true;
    }

    return false;
}

void game_rec_do_record_frame()
{
    game_audio_frame();
    game_velo_frame();

    svr_frame();

    game_state.rec_num_frames++;

    game_wind_update();
    game_rec_update_timeout();
}
