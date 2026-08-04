#include "game_priv.h"
#include "game_common.h"

// Needed to prevent the window from even showing.
BOOL WINAPI game_studio_set_window_pos_override(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    char class_name[64];
    GetClassNameA(hWnd, class_name, SVR_ARRAY_SIZE(class_name));

    if (!strcmp(class_name, "Valve001"))
    {
        uFlags = SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOREPOSITION | SWP_NOSENDCHANGING;
    }

    using OrgFn = decltype(game_studio_set_window_pos_override)*;
    OrgFn org_fn = (OrgFn)game_state.set_window_pos_hook.original;
    return org_fn(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

void game_studio_early_init()
{
    const char* studio_arg_pos = strstr(GetCommandLineA(), "--studio_h");

    if (studio_arg_pos == NULL)
    {
        // No studio used.
        return;
    }

    u32 studio_h = 0;
    sscanf(studio_arg_pos, "--studio_h %u", &studio_h);

    if (studio_h == 0)
    {
        // Just ignore.
        return;
    }

    // Not sure how to verify this parameter.
    // We inherit handles when creating this process so we can just read the handle address directly.
    // All handles only have 32 bits significant so this is safe in both 32 and 64 bit.
    game_state.studio_shared_mem_h = (HANDLE)studio_h;

    game_state.set_window_pos_ov.target = SetWindowPos;
    game_state.set_window_pos_ov.override = game_studio_set_window_pos_override;

#ifndef SVR_DEBUG
    game_hook_create(&game_state.set_window_pos_ov, &game_state.set_window_pos_hook);
    game_hook_enable(&game_state.set_window_pos_hook, true);
#endif
}

bool game_studio_init()
{
    bool ret = false;

    if (!game_studio_active())
    {
        ret = true;
        goto rexit;
    }

    // At this point, the shared memory will already have some data already filled in.
    game_state.studio_shared_ptr = (StudioSharedMem*)MapViewOfFile(game_state.studio_shared_mem_h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

    if (game_state.studio_shared_ptr == NULL)
    {
        DWORD error = GetLastError();

        svr_log("ERROR: Could not view studio shared memory (%lu). Full command line: %s\n", error, GetCommandLineA());
        goto rfail;
    }

    // Mute the process for this session when using studio.
    // This legacy function works fine for this, and doesn't require the WASAPI COM cascade.
    // The original volume is automatically restored when the process starts normally next time.
    //
    // ACTUALLY: Muting through this function causes permanent volume loss for most users when restarting the game, even without studio.
    // The volume mixer shows that the process is not muted. I can not personally reproduce this but for now don't mute the process at all.
    // Set to max volume to restore any pending issues.
    waveOutSetVolume(NULL, UINT32_MAX);

    game_state.studio_peer = &game_state.studio_shared_ptr->game_peer;

    svr_console_msg_and_log("Game (Studio): Connected to SVR Studio (session %d)\n", svr_atom_load(&game_state.studio_shared_ptr->session));

    // Notify started.
    SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);

    ret = true;
    goto rexit;

rfail:
rexit:
    return ret;
}

void game_studio_update()
{
    if (!game_studio_active())
    {
        return;
    }

    if (!game_state.studio_started_con_log)
    {
        game_engine_client_command(svr_va("con_logfile svr_con_log%d.txt\n", svr_atom_load(&game_state.studio_shared_ptr->session)));
        game_state.studio_started_con_log = true;
    }

    game_studio_update_pending_cmd();

    if (svr_movie_active())
    {
        if (svr_atom_load(&game_state.studio_shared_ptr->cancel))
        {
            game_rec_end_movie();
        }
    }

    s32 cmd_id = svr_atom_swap(&game_state.studio_peer->pending_cmd, STUDIO_SHARED_CMD_NONE);

    if (cmd_id == STUDIO_SHARED_CMD_NONE)
    {
        return;
    }

    svr_log("Game (Studio): Received command: %d\n", cmd_id);

    switch (cmd_id)
    {
        case STUDIO_SHARED_CMD_SKIP_TO_START:
        {
            StudioSharedSkipToStartCmd* cmd = &game_state.studio_peer->cmd_data.skip_to_start_cmd;

            // Returns when the target tick is reached.
            game_state.studio_pending_cmd = cmd_id;

            // Go to set tick (%d), not relative (0) and pause (1).
            game_engine_client_command(svr_va("demo_gototick %d 0 1\n", cmd->tick));

            // Set free roaming observer mode and allow super fast forward.
            game_engine_client_command("spec_mode 6; host_framerate 1; r_norefresh 1\n");
            break;
        }

        case STUDIO_SHARED_CMD_START_REC:
        {
            StudioSharedStartRecCmd* cmd = &game_state.studio_peer->cmd_data.start_rec_cmd;

            // Returns when the recording ends.
            game_state.studio_pending_cmd = cmd_id;

            if (cmd->steam_id[0])
            {
                // Spectating by Steam ID.
                // This only works if Steam is running, unfortunately.
                game_engine_client_command(svr_va("spec_player \"%s\"\n", cmd->steam_id));

                // Set first person observer mode, hide spectator menu and resume.
                game_engine_client_command("spec_mode 4; spec_menu 0; demo_resume\n");
            }

            else
            {
                game_engine_client_command("sm_start_replay");
            }

            game_engine_client_command(svr_va("startmovie %s timeout=%d profile=%s\n", cmd->movie_name, cmd->movie_length, cmd->profile));

            game_state.studio_spec_skips = 0;
            game_state.studio_next_spec_skip = 4;
            break;
        }

        case STUDIO_SHARED_CMD_PLAY_DEMO:
        {
            StudioSharedPlayDemoCmd* cmd = &game_state.studio_peer->cmd_data.play_demo_cmd;

            // Returns when the demo starts.
            game_state.studio_pending_cmd = cmd_id;

            game_engine_client_command(svr_va("playdemo %s\n", cmd->demo_name));
            break;
        }

        case STUDIO_SHARED_CMD_PLAY_REPLAY:
        {
            StudioSharedPlayReplayCmd* cmd = &game_state.studio_peer->cmd_data.play_replay_cmd;

            // Returns when the replay starts.
            game_state.studio_pending_cmd = cmd_id;

            game_state.studio_rv_rec_state = GAME_RV_REC_STATE_WAITING_FOR_MAP;

            svr_log("Game (Studio): Loading replay %s for map %s\n", cmd->replay_name, cmd->map_name);

            // It's really stupid but SourceMod is only loaded when a server is started so we can't check if replay viewer is loaded yet.
            game_engine_client_command(svr_va("map %s\n", cmd->map_name));
            break;
        }

        default:
        {
            svr_copy_string(svr_va("Game (Studio): Unknown command: %d", cmd_id), game_state.studio_peer->error, SVR_ARRAY_SIZE(StudioSharedPeer::error));
            SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
            break;
        }
    }
}

// For commands which require longer time to process.
void game_studio_update_pending_cmd()
{
    if (game_state.studio_pending_cmd == STUDIO_SHARED_CMD_NONE)
    {
        return;
    }

    switch (game_state.studio_pending_cmd)
    {
        case STUDIO_SHARED_CMD_SKIP_TO_START:
        {
            // The command finishes when the target tick has been reached.

            StudioSharedSkipToStartCmd* cmd = &game_state.studio_peer->cmd_data.skip_to_start_cmd;

            s32 cur_playback_tick = game_get_demo_player_playback_tick();

            if (cur_playback_tick >= cmd->tick)
            {
                // We have reached the destination, remove super fast forward.
                game_engine_client_command("host_framerate 0; r_norefresh 0\n");

                // Command finished.
                game_state.studio_pending_cmd = STUDIO_SHARED_CMD_NONE;
                SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
            }

            break;
        }

        case STUDIO_SHARED_CMD_START_REC:
        {
            // The command finishes when the recording is finished, which is determined by the timeout sent
            // to the startmovie command.

            StudioSharedStartRecCmd* cmd = &game_state.studio_peer->cmd_data.start_rec_cmd;

            if (game_state.studio_spec_skips < 4)
            {
                game_state.studio_next_spec_skip--;

                if (game_state.studio_next_spec_skip == 0)
                {
                    const char* toggle_commands[] = { "spec_mode 6\n", "spec_mode 4\n" };
                    game_engine_client_command(toggle_commands[game_state.studio_spec_skips & 1]);

                    game_state.studio_spec_skips++;
                    game_state.studio_next_spec_skip = 4;
                }
            }

            break;
        }

        case STUDIO_SHARED_CMD_PLAY_DEMO:
        {
            // The command finishes when the game is connected.

            StudioSharedPlayDemoCmd* cmd = &game_state.studio_peer->cmd_data.play_demo_cmd;

            s32 state = game_get_signon_state();

            if (state == game_state.search_desc.signon_state_full)
            {
                // Command finished.
                game_state.studio_pending_cmd = STUDIO_SHARED_CMD_NONE;
                SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
            }

            break;
        }

        case STUDIO_SHARED_CMD_PLAY_REPLAY:
        {
            // The command finishes when the replay is loaded.

            StudioSharedPlayReplayCmd* cmd = &game_state.studio_peer->cmd_data.play_replay_cmd;

            s32 signon_state = game_get_signon_state();

            if (game_state.studio_rv_rec_state == GAME_RV_REC_STATE_WAITING_FOR_MAP)
            {
                if (signon_state == game_state.search_desc.signon_state_full)
                {
                    svr_log("Game (Studio): Replay map loaded\n");

                    // There might not be a way to detect this earlier because we don't know when exactly the replay viewer extension is loaded.
                    // It is pretty stupid that this has to be determined after starting everything up and loading a map.
                    if (game_studio_find_replay_viewer())
                    {
                        svr_log("Game (Studio): Replay viewer found\n");

                        game_state.studio_rv_rec_state = GAME_RV_REC_STATE_WAITING_FOR_EXTENSION_READY;
                    }

                    else
                    {
                        game_state.studio_rv_rec_state = GAME_RV_REC_STATE_ERROR;

                        svr_log("Game (Studio): Replay viewer not found\n");

                        svr_copy_string("Game (Studio): Replay viewer not present", game_state.studio_peer->error, SVR_ARRAY_SIZE(StudioSharedPeer::error));
                        SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
                    }
                }
            }

            else if (game_state.studio_rv_rec_state == GAME_RV_REC_STATE_WAITING_FOR_EXTENSION_READY)
            {
                if (game_state.studio_rv_shared_ptr->state == GAME_RV_STATE_READY)
                {
                    svr_log("Game (Studio): Replay viewer extension ready\n");

                    game_state.studio_rv_rec_state = GAME_RV_REC_STATE_WAITING_FOR_REPLAY_LOADED;

                    // Dismiss the join team menu and join the spectate team (very sneaky).

                    game_engine_client_command("hidepanel all; spectate\n");
                    game_engine_client_command(svr_va("sm_load_replay \"%s\"\n", cmd->replay_name));
                }
            }

            else if (game_state.studio_rv_rec_state == GAME_RV_REC_STATE_WAITING_FOR_REPLAY_LOADED)
            {
                if (game_state.studio_rv_shared_ptr->state == GAME_RV_STATE_LOADED)
                {
                    svr_log("Game (Studio): Replay loaded and ready\n");

                    game_state.studio_rv_rec_state = GAME_RV_REC_STATE_WAITING_FOR_REPLAY_FINISHED;

                    // Command finished.
                    game_state.studio_pending_cmd = STUDIO_SHARED_CMD_NONE;
                    SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
                }
            }

            break;
        }

        default:
        {
            assert(false);
            break;
        }
    }
}

void game_studio_stop_recording()
{
    if (!game_studio_active())
    {
        return;
    }

    assert(game_state.studio_pending_cmd == STUDIO_SHARED_CMD_START_REC);

    // Command finished.
    game_state.studio_pending_cmd = STUDIO_SHARED_CMD_NONE;

    svr_console_msg_and_log("Game (Studio): Stopping\n");
    SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
}

bool game_studio_active()
{
    return game_state.studio_shared_mem_h;
}

void game_studio_movie_start_failed()
{
    if (!game_studio_active())
    {
        return;
    }

    assert(game_state.studio_pending_cmd == STUDIO_SHARED_CMD_START_REC);

    // Command finished.
    game_state.studio_pending_cmd = STUDIO_SHARED_CMD_NONE;

    SVR_COPY_STRING("Could not start movie. See svr_log.txt for details", game_state.studio_peer->error);
    SetEvent((HANDLE)game_state.studio_peer->wake_studio_h);
}

bool game_studio_find_replay_viewer()
{
    // Check for optional replay viewer support.
    // The shared memory is created by the replay viewer SourceMod extension.

    if (game_state.studio_rv_shared_ptr)
    {
        // We have it captain!
        return true;
    }

    game_state.studio_rv_shared_mem_h = OpenFileMappingA(FILE_MAP_READ, FALSE, "SVR_RV_SHARED_MEM");

    if (game_state.studio_rv_shared_mem_h)
    {
        game_state.studio_rv_shared_ptr = (GameReplayViewerSharedMem*)MapViewOfFile(game_state.studio_rv_shared_mem_h, FILE_MAP_READ, 0, 0, 0);

        if (game_state.studio_rv_shared_ptr)
        {
            return true;
        }

        else
        {
            DWORD error = GetLastError();
            svr_log("ERROR: Could not view replay viewer shared memory (%lu)\n", error);
        }
    }

    else
    {
        svr_log("ERROR: Replay viewer shared memory not found. Replays will not play\n");
    }

    return false;
}

bool game_has_studio_and_replay_viewer()
{
    return game_state.studio_rv_shared_ptr;
}
