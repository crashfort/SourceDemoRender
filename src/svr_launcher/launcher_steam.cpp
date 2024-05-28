#include "launcher_priv.h"

bool LauncherState::steam_find_path()
{
    HKEY steam_hkey = NULL;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &steam_hkey) != 0)
    {
        launcher_log("Steam is not installed.");
        return false;
    }

    DWORD steam_path_size = MAX_PATH;

    // These have to exist otherwise Steam wouldn't work.

    RegGetValueA(steam_hkey, NULL, "SteamPath", RRF_RT_REG_SZ, NULL, steam_path, &steam_path_size);

    for (DWORD i = 0; i < steam_path_size; i++)
    {
        if (steam_path[i] == '/')
        {
            steam_path[i] = '\\';
        }
    }

    RegCloseKey(steam_hkey);
    return true;
}

bool LauncherState::steam_find_libraries()
{
    bool ret = false;

    char full_vdf_path[MAX_PATH];
    SVR_SNPRINTF(full_vdf_path, "%s\\steamapps\\libraryfolders.vdf", steam_path);

    SvrVdfSection* vdf_root = svr_vdf_load(full_vdf_path);

    if (vdf_root == NULL)
    {
        launcher_log("No Steam libraries could be found.");
        goto rfail;
    }

    SvrVdfSection* libraries_folders_section = svr_vdf_section_find_section(vdf_root, "libraryfolders", NULL);

    if (libraries_folders_section == NULL)
    {
        goto rfail;
    }

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

        char* full_path = svr_dup_str(svr_va("%s\\steamapps", new_path));

        steam_library_paths.push(full_path);
    }

    ret = true;

rfail:
rexit:
    if (vdf_root)
    {
        svr_vdf_free(vdf_root);
    }

    return ret;
}

// Find the library and path where a game is installed.
// The game_steam_path parameter should be the steam_path parameter in the ini file, which is relative to the steamapps folder.
// Returns NULL if the game is not installed in the system.
char* LauncherState::steam_get_game_path_in_any_library(const char* game_steam_path)
{
    char path[MAX_PATH];

    for (s32 i = 0; i < steam_library_paths.size; i++)
    {
        SVR_SNPRINTF(path, "%s\\%s", steam_library_paths[i], game_steam_path);

        if (svr_does_file_exist(path))
        {
            // Game is in this library. We now have the full path.

            char* ret = svr_dup_str(path);
            return ret;
        }
    }

    return NULL;
}
