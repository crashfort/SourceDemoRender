#include "game_priv.h"

GameState game_state;

DWORD CALLBACK game_init_async_thread_proc(LPVOID param)
{
    game_init_async_proc();
    return 0; // Not used.
}

void game_init(SvrGameInitData* init_data)
{
    game_state.main_thread_id = GetCurrentThreadId();
    game_state.svr_path = init_data->svr_path;

    game_hook_init();

    game_studio_early_init();
    game_wind_early_init();

    // Init needs to be done async because we need to wait for the libraries to load while the game loads as normal.
    HANDLE h = CreateThread(NULL, 0, game_init_async_thread_proc, NULL, 0, NULL);
    CloseHandle(h);
}

void game_init_log()
{
    char log_file_path[MAX_PATH];
    SVR_SNPRINTF(log_file_path, "%s\\data\\svr_log.txt", game_state.svr_path);

    // Append to the log file the launcher created.
    svr_init_log(log_file_path, true);

    // Need to notify that we have started because a lot of things can go wrong in standalone launch.
    svr_log("Hello from the game\n");
}

void game_init_error(const char* format, ...)
{
    assert(GetCurrentThreadId() != game_state.main_thread_id); // Only to be used in the init thread.

    char message[1024];

    va_list va;
    va_start(va, format);
    SVR_VSNPRINTF(message, format, va);
    va_end(va);

    svr_log("!!! ERROR: %s\n", message);

    // Try and hide the game window if we have it.
    // This needs to be done because we are in a separate thread here, and we want the main thread to block as well.
    // Since we cannot add new custom messages and adjust the game window message loop, this is another way to prevent that.
    // We don't want the main window to be interacted with when this dialog is opened.

    HWND hwnd = NULL;
    EnumThreadWindows(game_state.main_thread_id, game_wind_enum_first_hwnd, (LPARAM)&hwnd);

    if (hwnd)
    {
        EnableWindow(hwnd, FALSE);
        ShowWindow(hwnd, SW_HIDE);
    }

    MessageBoxA(NULL, message, "SVR", MB_ICONERROR | MB_OK | MB_TASKMODAL);

    ExitProcess(1);
}

// In init thread.
void game_init_async_proc()
{
    game_init_log();

    game_search_wait_for_libs();

    svr_console_init();
    svr_prof_init();

    game_search_fill_desc(&game_state.search_desc);

    game_init_check_caps();
    game_overrides_init();
    game_proxies_init();
    game_video_init();
    game_audio_init();
    game_wind_init();
    game_rec_init();
    game_studio_init();

    IUnknown* video_device = game_state.video_desc->get_game_device();

    if (!svr_init(game_state.svr_path, video_device))
    {
        game_init_error("Could not initialize SVR. Ensure you are using the latest version of SVR and upload your svr_log.txt.");
    }

    game_hook_enable_all();

    // It's useful to show that we have loaded when in standalone mode.
    // This message may not be the latest message but at least it's in there.

    svr_console_msg("-------------------------------------------------------\n");
    svr_console_msg("SVR initialized\n");
    svr_console_msg("-------------------------------------------------------\n");
}

struct GameCapsPrint
{
    GameCaps cap;
    const char* name;
};

void game_init_check_caps()
{
#define CAP(X) GameCapsPrint { X, #X }

    GameCapsPrint caps_print[] =
    {
        CAP(GAME_CAP_HAS_CORE),
        CAP(GAME_CAP_HAS_LOCAL_PLAYER),
        CAP(GAME_CAP_HAS_AUDIO),
        CAP(GAME_CAP_HAS_VIDEO),
        CAP(GAME_CAP_HAS_AUTOSTOP),
        CAP(GAME_CAP_D3D9EX_VIDEO),
        CAP(GAME_CAP_AUDIO_DEVICE_1),
        CAP(GAME_CAP_AUDIO_DEVICE_1_5),
        CAP(GAME_CAP_AUDIO_DEVICE_2),
        CAP(GAME_CAP_64_BIT_AUDIO_TIME),
        CAP(GAME_CAP_LOCAL_PLAYER_DUMB),
        CAP(GAME_CAP_LOCAL_PLAYER_SMART),
        CAP(GAME_CAP_HAS_LAGCOMP),
        CAP(GAME_CAP_HAS_STUDIO),
    };

#undef CAP

    svr_log("Game caps:\n");

    for (s32 i = 0; i < SVR_ARRAY_SIZE(caps_print); i++)
    {
        GameCapsPrint* p = &caps_print[i];

        if (game_state.search_desc.caps & p->cap)
        {
            svr_log("- %s\n", p->name);
        }
    }

    GameCaps required_caps = GAME_CAP_HAS_CORE | GAME_CAP_HAS_VIDEO;

    if ((game_state.search_desc.caps & required_caps) != required_caps)
    {
        game_init_error("SVR support is missing or wrong. Ensure you are using the latest version of SVR and upload your svr_log.txt.");
    }
}

// Called when launching by the standalone launcher. This is before the process has started, and there are no game libraries loaded here.
extern "C" __declspec(dllexport) void svr_init_from_launcher(SvrGameInitData* init_data)
{
    game_init(init_data);
}
