#pragma once

struct LauncherGame
{
    char* file_name; // File name of ini.
    char* display_name; // Display name.
    char* path; // Path to executable.
    char* args; // Extra stuff to put in the start args.
};

struct LauncherState
{
    // -----------------------------------------------
    // Program state:

    // Our directory where we are running from. The game needs to know this.
    // This does not end with a slash.
    char working_dir[MAX_PATH];

    SvrDynArray<LauncherGame> game_list;

    void init();

    void launcher_log(const char* format, ...);
    __declspec(noreturn) void launcher_error(const char* format, ...);
    s32 get_choice_from_user(s32 min, s32 max);
    s32 start_game(LauncherGame* game);
    s32 autostart_game(const char* id);
    void load_games();
    bool parse_game(const char* file, LauncherGame* dest);
    void free_game(LauncherGame* game);
    s32 show_start_menu();
    bool exe_is_right_arch(const char* path);

    // -----------------------------------------------
    // Steam state:

    // Path to the main Steam installation. This does not end with a slash.
    // The libraries are in steam_library_paths.
    char steam_path[MAX_PATH];

    // A Steam library can be installed anywhere, we have to iterate over all of them to see where a game is located.
    // These do not end with a slash.
    SvrDynArray<char*> steam_library_paths;

    bool steam_find_path();
    bool steam_find_libraries();
    char* steam_get_game_path_in_any_library(const char* game_steam_path);

    // -----------------------------------------------
    // System state:

    void sys_show_windows_version();
    void sys_show_processor();
    void sys_show_available_memory();
    void sys_check_hw_caps();

    // -----------------------------------------------
    // IPC state:

    void ipc_setup_in_remote_process(LauncherGame* game, HANDLE process, HANDLE thread);
};
