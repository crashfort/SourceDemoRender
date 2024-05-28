#include "launcher_priv.h"

// Base arguments that every game will have.
const char* BASE_GAME_ARGS = "-steam -insecure +sv_lan 1 -console -novid";

s32 LauncherState::start_game(LauncherGame* game)
{
    // We don't need the game directory necessarily (mods work differently) since we apply the -game parameter.
    // All known Source games will use SetCurrentDirectory to the mod (game) directory anyway.

    char full_args[1024];
    full_args[0] = 0;

    StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), BASE_GAME_ARGS); // Always add base args.

    // Add other args from game too.
    if (game->args)
    {
        StringCchCatA(full_args, SVR_ARRAY_SIZE(full_args), svr_va(" %s", game->args));
    }

    launcher_log("Starting %s (%s). If launching doesn't work then make sure any antivirus is disabled\n", game->display_name, game->file_name);

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION info;

    if (!CreateProcessA(game->path, full_args, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &start_info, &info))
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

s32 LauncherState::autostart_game(const char* id)
{
    LauncherGame* found_game = NULL;

    for (s32 i = 0; i < game_list.size; i++)
    {
        LauncherGame* game = &game_list[i];

        if (!strcmpi(id, game->file_name))
        {
            found_game = game;
            break;
        }
    }

    if (found_game == NULL)
    {
        launcher_error("Cannot autostart, no game with id %s was found.", id);
    }

    return start_game(found_game);
}

// Load and parse all games.
void LauncherState::load_games()
{
    char pattern[MAX_PATH];
    SVR_SNPRINTF(pattern, "%s\\data\\games\\*.ini", working_dir);

    WIN32_FIND_DATAA ffd;
    auto h = FindFirstFileExA(pattern, FindExInfoBasic, &ffd, FindExSearchNameMatch, NULL, 0);

    if (h == INVALID_HANDLE_VALUE)
    {
        // Doesn't even exist.
        launcher_error("No games to launch.");
    }

    while (true)
    {
        // Why would anyone ever want to iterate over these?
        if (!strcmp(ffd.cFileName, ".") || !strcmp(ffd.cFileName, ".."))
        {
        }

        else
        {
            char full_file_path[MAX_PATH];
            SVR_SNPRINTF(full_file_path, "%s\\data\\games\\%s", working_dir, ffd.cFileName);

            // Should not be searching recursively.
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                LauncherGame game = {};

                if (parse_game(full_file_path, &game))
                {
                    game_list.push(game);
                }
            }
        }

        if (FindNextFileA(h, &ffd) == 0)
        {
            break;
        }
    }

    FindClose(h);
}

bool LauncherState::parse_game(const char* file, LauncherGame* dest)
{
    bool ret = false;

    SvrIniSection* ini_root = svr_ini_load(file);

    if (ini_root == NULL)
    {
        goto rfail;
    }

    SvrIniKeyValue* name_kv = svr_ini_section_find_kv(ini_root, "name");
    SvrIniKeyValue* steam_path_kv = svr_ini_section_find_kv(ini_root, "steam_path");
    SvrIniKeyValue* other_path_kv = svr_ini_section_find_kv(ini_root, "other_path");
    SvrIniKeyValue* args_kv = svr_ini_section_find_kv(ini_root, "args");

    if (name_kv == NULL)
    {
        svr_log("WARNING: Game %s is missing the name key and value\n", file);
        goto rfail;
    }

    // At least one must be specified.
    if (steam_path_kv == NULL && other_path_kv == NULL)
    {
        svr_log("WARNING: Game %s is missing the Steam path or other path key and value\n", file);
        goto rfail;
    }

    // Both cannot be specified because we don't know which one to use.
    if (steam_path_kv && other_path_kv)
    {
        svr_log("WARNING: Game %s has both the Steam path and other path key and value\n", file);
        goto rfail;
    }

    if (steam_path_kv)
    {
        // Game must be installed in one of the Steam libraries.
        dest->path = steam_get_game_path_in_any_library(steam_path_kv->value);
    }

    if (other_path_kv)
    {
        // Must be an absolute path.
        if (PathIsRelativeA(other_path_kv->value))
        {
            svr_log("WARNING: Game %s must not use a relative path in the other_path key value\n", file);
            goto rfail;
        }

        if (svr_does_file_exist(other_path_kv->value))
        {
            dest->path = svr_dup_str(other_path_kv->value);
        }
    }

    // Must have a path.
    if (dest->path == NULL)
    {
        goto rfail;
    }

    // Filter out x86 and x64.
    if (!exe_is_right_arch(dest->path))
    {
        goto rfail;
    }

    if (args_kv)
    {
        dest->args = svr_dup_str(args_kv->value);
    }

    dest->file_name = svr_dup_str(PathFindFileNameA(file));
    dest->display_name = svr_dup_str(name_kv->value);

    ret = true;
    goto rexit;

rfail:
    free_game(dest);

rexit:
    if (ini_root)
    {
        svr_ini_free(ini_root);
        ini_root = NULL;
    }

    return ret;
}

void LauncherState::free_game(LauncherGame* game)
{
    if (game->file_name)
    {
        svr_free(game->file_name);
        game->file_name = NULL;
    }

    if (game->display_name)
    {
        svr_free(game->display_name);
        game->display_name = NULL;
    }

    if (game->path)
    {
        svr_free(game->path);
        game->path = NULL;
    }

    if (game->args)
    {
        svr_free(game->args);
        game->args = NULL;
    }
}

s32 LauncherState::show_start_menu()
{
    launcher_log("Installed games:\n");

    for (s32 i = 0; i < game_list.size; i++)
    {
        LauncherGame* game = &game_list[i];
        launcher_log("[%d] %s (%s)\n", i + 1, game->display_name, game->file_name);
    }

    printf("\nSelect which game to start: ");

    s32 selection = get_choice_from_user(0, game_list.size);

    if (selection < 0)
    {
        return 1;
    }

    LauncherGame* game = &game_list[selection];
    return start_game(game);
}

bool LauncherState::exe_is_right_arch(const char* path)
{
    DWORD exe_type = 0;

    if (!GetBinaryTypeA(path, &exe_type))
    {
        svr_log("GetBinaryTypeA failed with code %lu\n", GetLastError());
        return false;
    }

#ifdef _WIN64
    if (exe_type != SCS_64BIT_BINARY)
    {
        return false;
    }
#else
    if (exe_type != SCS_32BIT_BINARY)
    {
        return false;
    }
#endif

    return true;
}
