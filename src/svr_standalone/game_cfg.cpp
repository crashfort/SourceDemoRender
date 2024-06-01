#include "game_priv.h"

bool game_has_cfg(const char* name)
{
    char full_cfg_path[MAX_PATH];
    SVR_SNPRINTF(full_cfg_path, "%s\\data\\cfg\\%s", game_state.svr_path, name);

    bool res = svr_does_file_exist(full_cfg_path);
    return res;
}

bool game_run_cfg(const char* name, bool required)
{
    char full_cfg_path[MAX_PATH];
    SVR_SNPRINTF(full_cfg_path, "%s\\data\\cfg\\%s", game_state.svr_path, name);

    // Commands must end with a newline.
    char* file_mem = svr_read_file_as_string(full_cfg_path, SVR_READ_FILE_FLAGS_NEW_LINE);

    if (file_mem == NULL)
    {
        if (required)
        {
            svr_log("ERROR: Could not open cfg %s\n", full_cfg_path);
        }

        return false;
    }

    svr_log("Running cfg %s\n", name);

    // The file can be executed as is. The game takes care of splitting by newline.
    // We don't monitor what is inside the cfg, it's up to the user.
    game_engine_client_command(file_mem);

    svr_free(file_mem);

    return true;
}

// Run all user cfgs for a given event (such as movie start or movie end).
void game_run_cfgs_for_event(const char* name)
{
    game_run_cfg(svr_va("svr_movie_%s.cfg", name), true);
    game_run_cfg(svr_va("svr_movie_%s_user.cfg", name), false);
}
