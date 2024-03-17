#include "launcher_priv.h"

LauncherState launcher_state;

int main(int argc, char** argv)
{
#ifdef SVR_DEBUG
    _set_error_mode(_OUT_TO_MSGBOX); // Must be called so we can actually use assert because Microsoft messed it up in console builds.
#endif

    // Enable this to generate the machine code for remote_func (see comments at that function).
#if 0
    void ipc_generate_bytes();
    ipc_generate_bytes();
    return 0;
#endif

    // For standalone mode, the launcher creates the log file that the game then appends to.
    svr_init_log("data\\SVR_LOG.txt", false);

    launcher_state.init();

    // We delay finding which Steam library a game is in until a game has been chosen.

    // Autostarting a game works by giving the app id.
    if (argc == 2)
    {
        SteamAppId app_id = strtoul(argv[1], NULL, 10);

        if (app_id == 0)
        {
            return 1;
        }

        return launcher_state.autostart_game(app_id);
    }

    return launcher_state.steam_show_start_menu();
}
