#include "game_priv.h"

const s32 GAME_WIND_TITLE_SIZE = 512;

BOOL CALLBACK game_wind_enum_first_hwnd(HWND hwnd, LPARAM lparam)
{
    HWND* out_hwnd = (HWND*)lparam;
    *out_hwnd = hwnd;
    return FALSE; // Just take the first one.
}

void game_wind_early_init()
{
    // Used by the window progress bar.
    // Must be called in the main thread because it will be used in the main thread.

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&game_state.wind_taskbar_list));
}

void game_wind_init()
{
    game_state.wind_def_title = (char*)svr_zalloc(sizeof(char) * GAME_WIND_TITLE_SIZE);

    // Find the main window. We could probably scan for this too.
    EnumThreadWindows(game_state.main_thread_id, game_wind_enum_first_hwnd, (LPARAM)&game_state.wind_hwnd);

    GetWindowTextA(game_state.wind_hwnd, game_state.wind_def_title, GAME_WIND_TITLE_SIZE);

    game_wind_reset();
}

void game_wind_free()
{
    if (game_state.wind_def_title)
    {
        svr_free(game_state.wind_def_title);
        game_state.wind_def_title = NULL;
    }

    if (game_state.wind_taskbar_list)
    {
        svr_release(game_state.wind_taskbar_list);
        game_state.wind_taskbar_list = NULL;
    }

    game_state.wind_hwnd = NULL;
}

// Update the window title to display the rendered video time, and elapsed real time.
void game_wind_update_title(s64 now)
{
    // Transform number of frames in a unit of frames per second into an elapsed period in microseconds.
    // This is the video time.
    SvrSplitTime video_split = svr_split_time(svr_rescale(game_state.rec_num_frames, 1000000, game_state.rec_game_rate));

    // This is the real elapsed time.
    SvrSplitTime real_split = svr_split_time(now - game_state.rec_start_time);

    char buf[1024];
    SVR_SNPRINTF(buf, "%02d:%02d.%03d (%02d:%02d:%02d)", video_split.minutes, video_split.seconds, video_split.millis, real_split.hours, real_split.minutes, real_split.seconds);

    SetWindowTextA(game_state.wind_hwnd, buf);
}

// Update the taskbar progress bar for region rendering.
void game_wind_update_progress(s64 now)
{
    if (game_state.rec_timeout == 0)
    {
        return; // Should not stop automatically, and no progress to update.
    }

    s64 end_frame = game_state.rec_timeout * game_state.rec_game_rate;

    game_state.wind_taskbar_list->SetProgressValue(game_state.wind_hwnd, game_state.rec_num_frames, end_frame);
}

void game_wind_update()
{
    s64 now = svr_prof_get_real_time();

    if (now < game_state.wind_next_update_time)
    {
        return;
    }

    // Need to throttle this because updating the window is slow apparently.
    game_state.wind_next_update_time = now + 500000;

    game_wind_update_title(now);
    game_wind_update_progress(now);
}

// Restore window title and progress bar.
void game_wind_reset()
{
    game_state.wind_next_update_time = 0;

    SetWindowTextA(game_state.wind_hwnd, game_state.wind_def_title);

    game_state.wind_taskbar_list->SetProgressValue(game_state.wind_hwnd, 0, 0);
    game_state.wind_taskbar_list->SetProgressState(game_state.wind_hwnd, TBPF_NOPROGRESS);
}
