#pragma once

const s32 MAX_STEAM_LIBRARIES = 32;

struct LauncherGame
{
    SteamAppId app_id;
    const char* name; // Display name shown on Steam.
    const char* extra_args; // Extra stuff to put in the start args.
    const char* root_dir; // Paths to append to each Steam library.
    const char* exe_name; // Where to find the executable built up from the Steam library path plus the game root directory (above).
    s32 build_id; // Build versions that have been tested (located in the appmanifest acf).
};

extern LauncherGame SUPPORTED_GAMES[];
extern const s32 NUM_SUPPORTED_GAMES;

struct LauncherState
{
    // -----------------------------------------------
    // Program state:

    // Our directory where we are running from. The game needs to know this.
    // This does not end with a slash.
    char working_dir[MAX_PATH];

    void init();

    void launcher_log(const char* format, ...);
    __declspec(noreturn) void launcher_error(const char* format, ...);
    s32 get_choice_from_user(s32 min, s32 max);
    char* get_custom_launch_params(LauncherGame* game);
    s32 start_game(LauncherGame* game);
    s32 autostart_game(SteamAppId app_id);

    // -----------------------------------------------
    // Steam state:

    HKEY steam_hkey;
    char steam_path[MAX_PATH];
    DWORD steam_active_user;

    // A Steam library can be installed anywhere, we have to iterate over all of them to see where a game is located.
    SvrDynArray<char*> steam_library_paths; // These end with a slash.

    SvrDynArray<LauncherGame*> steam_installed_games; // Games that are installed on Steam right now.

    void steam_find_path();
    void steam_find_libraries();
    void steam_find_installed_supported_games();
    char* steam_get_launch_params(LauncherGame* game);
    void steam_find_game_paths(LauncherGame* game, char** game_path, char** acf_path);
    void steam_find_game_build(LauncherGame* game, const char* acf_path, s32* build_id);
    void steam_test_game_build_against_known(LauncherGame* game, s32 build_id);
    s32 steam_show_start_menu();

    // -----------------------------------------------
    // System state:

    void sys_show_windows_version();
    void sys_show_processor();
    void sys_show_available_memory();
    void sys_check_hw_caps();

    // -----------------------------------------------
    // IPC state:

    void ipc_setup_in_remote_process(LauncherGame* game, HANDLE process, HANDLE thread);

    // -----------------------------------------------
    // Launcher state:
};
