#include "launcher_priv.h"

// Base arguments that every game will have.
const char* BASE_GAME_ARGS = "-steam -insecure +sv_lan 1 -console -novid";

char* LauncherState::get_custom_launch_params(LauncherGame* game)
{
    char* ret = NULL;

    SvrIniSection* ini_root = svr_ini_load("svr_launch_params.ini");

    if (ini_root == NULL)
    {
        return ret;
    }

    SvrIniKeyValue* found_kv = svr_ini_section_find_kv(ini_root, svr_va("%u", game->app_id));

    if (found_kv)
    {
        ret = svr_dup_str(found_kv->value);
    }

    svr_ini_free(ini_root);

    return ret;
}

s32 LauncherState::start_game(LauncherGame* game)
{
    // We don't need the game directory necessarily (mods work differently) since we apply the -game parameter.
    // All known Source games will use SetCurrentDirectory to the mod (game) directory anyway.

    char full_exe_path[MAX_PATH];
    full_exe_path[0] = 0;

    char* installed_game_path = NULL;
    char* game_acf_path = NULL;

    steam_find_game_paths(game, &installed_game_path, &game_acf_path);

    s32 game_build_id = 0;
    steam_find_game_build(game, game_acf_path, &game_build_id);

    SVR_SNPRINTF(full_exe_path, "%s%s", installed_game_path, game->exe_name);

    char full_args[1024];
    full_args[0] = 0;

    StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), BASE_GAME_ARGS);

    // Prioritize custom args over Steam args.

    char* extra_args = get_custom_launch_params(game);

    if (extra_args == NULL)
    {
        extra_args = steam_get_launch_params(game);
    }

    if (extra_args)
    {
        StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), svr_va(" %s", extra_args));
    }

    // Use the written game arg if the user params don't specify it.

    const char* custom_game_arg = strstr(full_args, "-game ");

    if (custom_game_arg == NULL)
    {
        StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), svr_va(" %s", game->extra_args));
    }

    // The user has provided their own game directory, just show it.
    else
    {
        char game_dir[MAX_PATH];
        game_dir[0] = 0;

        const char* ptr = svr_advance_until_whitespace(custom_game_arg);
        ptr = svr_advance_until_after_whitespace(ptr);
        svr_extract_string(ptr, game_dir, SVR_ARRAY_SIZE(game_dir));

        if (game_dir[0] != 0)
        {
            launcher_log("Using %s as custom game override\n", game_dir);
        }

        else
        {
            launcher_error("Custom -game parameter in Steam or svr_launch_params.ini is missing the value. The value should be the mod name.");
        }
    }

    steam_test_game_build_against_known(game, game_build_id);

    launcher_log("Starting %s (build %d). If launching doesn't work then make sure any antivirus is disabled\n", game->name, game_build_id);

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION info;

    if (!CreateProcessA(full_exe_path, full_args, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &start_info, &info))
    {
        svr_log("CreateProcessA failed with code %lu\n", GetLastError());
        launcher_error("Could not initialize standalone SVR. If you use an antivirus, add exception or disable.");
    }

    ipc_setup_in_remote_process(game, info.hProcess, info.hThread);

    svr_log("Launcher finished, rest of the log is from the game\n");
    svr_log("---------------------------------------------------\n");

    // Need to close the file so the game can open it.
    svr_shutdown_log();

    // Let the process actually start now.
    // You want to place a breakpoint on this line when debugging the game!
    // When this breakpoint is hit, attach to the game process and then continue this process.
    ResumeThread(info.hThread);

    CloseHandle(info.hThread);

    // We don't have to wait here since we don't print to the launcher console from the game anymore.
    // WaitForSingleObject(info.hProcess, INFINITE);

    CloseHandle(info.hProcess);

    return 0;
}

s32 LauncherState::autostart_game(SteamAppId app_id)
{
    LauncherGame* found_game = NULL;

    const s32 SUPPORTED = 1 << 0;
    const s32 INSTALLED = 1 << 1;

    s32 flags = 0;

    for (s32 i = 0; i < steam_installed_games.size; i++)
    {
        LauncherGame* game = steam_installed_games[i];

        if (app_id == game->app_id)
        {
            found_game = game;
            flags |= INSTALLED | SUPPORTED;
            break;
        }
    }

    if (found_game == NULL)
    {
        // Show why this game cannot be autostarted.

        for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
        {
            LauncherGame* game = &SUPPORTED_GAMES[i];

            if (app_id == game->app_id)
            {
                flags |= SUPPORTED;
                break;
            }
        }

        if (!(flags & SUPPORTED))
        {
            launcher_error("Cannot autostart, app id %u is not supported.", app_id);
        }

        if (!(flags & INSTALLED))
        {
            launcher_error("Cannot autostart, app id %u is not installed.", app_id);
        }
    }

    return start_game(found_game);
}
