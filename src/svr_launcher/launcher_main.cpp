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

    // Autostarting a game works by giving the id.
    if (argc == 2)
    {
        const char* id = argv[1];
        return launcher_state.autostart_game(id);
    }

    return launcher_state.show_start_menu();
}
