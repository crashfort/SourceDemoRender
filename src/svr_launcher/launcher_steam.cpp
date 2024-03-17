#include "launcher_priv.h"

void LauncherState::steam_find_path()
{
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &steam_hkey) != 0)
    {
        launcher_error("Steam is not installed.");
    }

    DWORD steam_path_size = MAX_PATH;
    DWORD steam_active_user_size = sizeof(DWORD);

    // These have to exist otherwise Steam wouldn't work.

    RegGetValueA(steam_hkey, NULL, "SteamPath", RRF_RT_REG_SZ, NULL, steam_path, &steam_path_size);
    RegGetValueA(steam_hkey, "ActiveProcess", "ActiveUser", RRF_RT_DWORD, NULL, &steam_active_user, &steam_active_user_size);

    // Not strictly true but we need Steam to be running in order to read the launch parameters to use.
    if (steam_active_user == 0)
    {
        launcher_error("Steam must be running for SVR to work.");
    }

    for (DWORD i = 0; i < steam_path_size; i++)
    {
        if (steam_path[i] == '/')
        {
            steam_path[i] = '\\';
        }
    }
}

void LauncherState::steam_find_libraries()
{
    char full_vdf_path[MAX_PATH];
    SVR_SNPRINTF(full_vdf_path, "%s\\steamapps\\libraryfolders.vdf", steam_path);

    SvrVdfSection* vdf_root = svr_vdf_load(full_vdf_path);

    if (vdf_root == NULL)
    {
        launcher_error("No Steam libraries could be found.");
    }

    SvrVdfSection* libraries_folders_section = svr_vdf_section_find_section(vdf_root, "libraryfolders", NULL);

    if (libraries_folders_section)
    {
        for (s32 lib_idx = 0; lib_idx < libraries_folders_section->sections.size; lib_idx++)
        {
            SvrVdfSection* lib_section = libraries_folders_section->sections[lib_idx];
            SvrVdfKeyValue* path_kv = svr_vdf_section_find_kv(lib_section, "path");

            if (path_kv == NULL)
            {
                continue;
            }

            // Paths in vdf will be escaped, we need to unescape.

            char new_path[MAX_PATH];
            new_path[0] = 0;

            svr_unescape_path(path_kv->value, new_path, SVR_ARRAY_SIZE(new_path));

            char* full_path = svr_dup_str(svr_va("%s\\steamapps\\", new_path));

            steam_library_paths.push(full_path);
        }
    }

    svr_vdf_free(vdf_root);
}

void LauncherState::steam_find_installed_supported_games()
{
    // We do a quick search through the registry to determine if we have the supported games.

    HKEY steam_apps_hkey;
    RegOpenKeyExA(steam_hkey, "Apps", 0, KEY_READ, &steam_apps_hkey);

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        LauncherGame* game = &SUPPORTED_GAMES[i];

        char buf[64];
        SVR_SNPRINTF(buf, "%u", game->app_id);

        HKEY game_hkey;

        if (RegOpenKeyExA(steam_apps_hkey, buf, 0, KEY_READ, &game_hkey) != 0)
        {
            // If we don't have this game.
            continue;
        }

        DWORD installed = 0;
        DWORD installed_size = sizeof(DWORD);

        RegGetValueA(game_hkey, NULL, "Installed", RRF_RT_REG_DWORD, NULL, &installed, &installed_size);

        if (installed == 0)
        {
            // We can only use installed games. Using the Steam service protocol (steam://install/240) we can install games but we probably
            // don't want to do that.

            continue;
        }

        steam_installed_games.push(game);
    }

    if (steam_installed_games.size == 0)
    {
        launcher_error("None of the supported games are installed on Steam.");
    }
}

// Use the launch parameters from Steam if we can.
char* LauncherState::steam_get_launch_params(LauncherGame* game)
{
    char* ret = NULL;

    char full_vdf_path[MAX_PATH];
    SVR_SNPRINTF(full_vdf_path, "%s\\userdata\\%s\\config\\localconfig.vdf", steam_path, svr_va("%lu", steam_active_user));

    SvrVdfSection* vdf_root = svr_vdf_load(full_vdf_path);

    if (vdf_root == NULL)
    {
        // Steam must be installed wrong if this fails.
        launcher_log("Could not read Steam user settings. Steam may be installed wrong.");
        return ret;
    }

    char buf[32];
    SVR_SNPRINTF(buf, "%u", game->app_id);

    const char* kv_path[] = { "userlocalconfigstore", "software", "valve", "steam", "apps", buf, "launchoptions" };

    SvrVdfKeyValue* launch_options_kv = svr_vdf_section_find_kv_path(vdf_root, kv_path, SVR_ARRAY_SIZE(kv_path));

    if (launch_options_kv)
    {
        ret = svr_dup_str(launch_options_kv->value);
    }

    else
    {
        launcher_log("Steam launch parameters for %s could not be found\n", game->name);
    }

    svr_vdf_free(vdf_root);

    return ret;
}

void LauncherState::steam_find_game_paths(LauncherGame* game, char** game_path, char** acf_path)
{
    for (s32 i = 0; i < steam_library_paths.size; i++)
    {
        char* lib = steam_library_paths[i];

        // If this file is available here then the game is installed in this library.

        char acf_file_path[MAX_PATH];
        SVR_SNPRINTF(acf_file_path, "%sappmanifest_%u.acf", lib, game->app_id);

        WIN32_FILE_ATTRIBUTE_DATA attr;

        if (GetFileAttributesExA(acf_file_path, GetFileExInfoStandard, &attr))
        {
            *game_path = svr_dup_str(svr_va("%s%s", lib, game->root_dir));
            *acf_path = svr_dup_str(acf_file_path);
            return;
        }
    }

    // Cannot happen unless Steam is installed wrong in which case it wouldn't work anyway.
    launcher_error("Game %s was not found in any Steam library. Steam may be installed wrong.", game->name);
}

void LauncherState::steam_find_game_build(LauncherGame* game, const char* acf_path, s32* build_id)
{
    SvrVdfSection* vdf_root = svr_vdf_load(acf_path);

    if (vdf_root == NULL)
    {
        launcher_log("Could not open appmanifest of game %s (%lu)\n", game->name, GetLastError());
        return;
    }

    const char* kv_path[] = { "appstate", "buildid" };
    SvrVdfKeyValue* build_kv = svr_vdf_section_find_kv_path(vdf_root, kv_path, SVR_ARRAY_SIZE(kv_path));

    if (build_kv)
    {
        *build_id = atoi(build_kv->value);
    }

    else
    {
        svr_log("Build number was not found in appmanifest of game %s\n", game->name);
    }

    svr_vdf_free(vdf_root);
}

void LauncherState::steam_test_game_build_against_known(LauncherGame* game, s32 build_id)
{
    if (build_id > 0)
    {
        if (build_id != game->build_id)
        {
            launcher_log("-----------------------------\n");
            launcher_log("WARNING: Mismatch between installed Steam build and tested SVR build. "
                         "Steam game build is %d and tested build is %d. "
                         "Problems may occur as this game build has not been tested!\n", build_id, game->build_id);
            launcher_log("-----------------------------\n");
        }
    }
}

s32 LauncherState::steam_show_start_menu()
{
    launcher_log("Installed games:\n");

    for (s32 i = 0; i < steam_installed_games.size; i++)
    {
        LauncherGame* game = steam_installed_games[i];
        launcher_log("[%d] %s\n", i + 1, game->name);
    }

    printf("\nSelect which game to start: ");

    s32 selection = get_choice_from_user(0, steam_installed_games.size);

    if (selection < 0)
    {
        return 1;
    }

    LauncherGame* game = steam_installed_games[selection];
    return start_game(game);
}
