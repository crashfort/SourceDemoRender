#include "svr_common.h"
#include "svr_defs.h"
#include <Windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "svr_logging.h"
#include "svr_vdf.h"
#include "svr_ini.h"
#include <VersionHelpers.h>
#include <stb_sprintf.h>
#include <d3d11.h>
#include <Psapi.h>
#include <WbemIdl.h>

struct FilePathA
{
    char path[MAX_PATH];
};

// Will put both to console and to file.
// Use printf for other messages that should not be shown in file.
// Use svr_log for messages that should not be shown on screen.
void launcher_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    svr_log_v(format, va);
    vprintf(format, va);
    va_end(va);
}

__declspec(noreturn) void launcher_error(const char* format, ...)
{
    char message[1024];

    va_list va;
    va_start(va, format);
    stbsp_vsnprintf(message, 1024, format, va);
    va_end(va);

    svr_log("!!! LAUNCHER ERROR: %s\n", message);

    MessageBoxA(NULL, message, "SVR", MB_TASKMODAL | MB_ICONERROR | MB_OK);

    ExitProcess(1);
}

struct SupportedGame
{
    SteamAppId app_id;
    const char* name; // Display name shown on Steam.
    const char* extra_args; // Extra stuff to put in the start args.
    const char* root_dir; // Paths to append to each Steam library.
    const char* exe_name; // Where to find the executable built up from the Steam library path plus the game root directory (above).
    s32 build_id; // Build versions that have been tested (located in the appmanifest acf).
};

SupportedGame SUPPORTED_GAMES[] = {
    SupportedGame { STEAM_GAME_CSS, "Counter-Strike: Source", "-game cstrike", "common\\Counter-Strike Source\\", "hl2.exe", 6946501 },
    SupportedGame { STEAM_GAME_CSGO, "Counter-Strike: Global Offensive", "-game csgo", "common\\Counter-Strike Global Offensive\\", "csgo.exe", 8128170 },
    SupportedGame { STEAM_GAME_TF2, "Team Fortress 2", "-game tf", "common\\Team Fortress 2\\", "hl2.exe", 7504322 },
    SupportedGame { STEAM_GAME_ZPS, "Zombie Panic! Source", "-game zps", "common\\Zombie Panic Source\\", "zps.exe", 5972042 },
    SupportedGame { STEAM_GAME_EMPIRES, "Empires", "-game empires", "common\\Empires\\", "hl2.exe", 8658619 },
    SupportedGame { STEAM_GAME_SYNERGY, "Synergy", "-game synergy", "common\\Synergy\\", "synergy.exe", 791804 },
    SupportedGame { STEAM_GAME_HL2, "Half-Life 2", "-game hl2", "common\\Half-Life 2\\", "hl2.exe", 4233294 },
    SupportedGame { STEAM_GAME_HL2DM, "Half-Life 2: Deathmatch", "-game hl2mp", "common\\Half-Life 2 Deathmatch\\", "hl2.exe", 6935373 },
    SupportedGame { STEAM_GAME_BMS, "Black Mesa", "-game bms", "common\\Black Mesa\\", "bms.exe", 4522431 },
    SupportedGame { STEAM_GAME_HDTF, "Hunt Down The Freeman", "-game hdtf", "common\\Hunt Down The Freeman\\", "hdtf.exe", 2604730 },
};

const s32 NUM_SUPPORTED_GAMES = SVR_ARRAY_SIZE(SUPPORTED_GAMES);

// Base arguments that every game will have.
const char* BASE_GAME_ARGS = "-steam -insecure +sv_lan 1 -console -novid";

const s32 MAX_STEAM_LIBRARIES = 32;

// Whether or not the games we support are installed.
bool steam_install_states[NUM_SUPPORTED_GAMES];
s32 num_installed_games;

// Supported games that are running that can be injected to.
bool running_games[NUM_SUPPORTED_GAMES];
HANDLE running_procs[NUM_SUPPORTED_GAMES];
s32 num_running_games;

// A Steam library can be installed anywhere, we have to iterate over all of them to see where a game is located.

s32 num_steam_libraries;
FilePathA steam_library_paths[MAX_STEAM_LIBRARIES];

// This is used to remap indexes when selecting games in the menu from games that are installed vs games that are supported.
s32 steam_index_remaps[NUM_SUPPORTED_GAMES];
s32 running_index_remaps[NUM_SUPPORTED_GAMES];

// Registry stuff.

HKEY steam_hkey;
char steam_path[MAX_PATH];
DWORD steam_active_user;

// Our directory where we are running from. The game needs to know this.
char working_dir[MAX_PATH];

using InitFuncType = void(__cdecl*)(SvrGameInitData* init_data);

// The structure that will be located in the started process.
// It is used as a parameter to the below function.
struct IpcStructure
{
    // Windows API functions.
    decltype(LoadLibraryA)* LoadLibraryA;
    decltype(GetProcAddress)* GetProcAddress;
    decltype(SetDllDirectoryA)* SetDllDirectoryA;

    // The name of the library to load.
    const char* library_name;

    // The initialization export function to call.
    const char* export_name;

    // The path of the SVR directory.
    const char* svr_path;

    // What Steam game we are starting.
    SteamAppId app_id;
};

// For injection we have to do another step with a remote thread because APC will not be run like at the start of a process.
struct IpcInjectStructure
{
    IpcStructure* param;
    PAPCFUNC func;
};

// This is the function that will be injected into the target process.
// The instructions remote_func_bytes below is the result of this function.
// You can use code listing in Visual Studio to generate the machine code for this:
// Using the property pages UI for svr_launcher_main.cpp, go to C/C++ -> Output Files -> Assembler Output and set it to Assembly With Machine Code (/FaC).
// Then compile the file and find the .cod file in the build directory. Open this and extract the bytes then paste.
// For the function to actually be compiled in, it must be referenced. Enable the generation code in main for this and build in Release.
// Note that MSVC sometimes doesn't always create this file or it is sometimes not updated properly. Rebuild the project until it works.
VOID CALLBACK remote_func(ULONG_PTR param)
{
    IpcStructure* data = (IpcStructure*)param;

    // There is no error handling here as there's no practical way to report
    // stuff back within this limited environment.
    // There have not been cases of these api functions failing with proper input
    // so let's stick with the simplest working solution for now.

    // Add our resource path as searchable to resolve library dependencies.
    data->SetDllDirectoryA(data->svr_path);

    // We need to call the right export in svr_game.dll.

    HMODULE module = data->LoadLibraryA(data->library_name);
    InitFuncType init_func = (InitFuncType)data->GetProcAddress(module, data->export_name);

    SvrGameInitData init_data;
    init_data.svr_path = data->svr_path;
    init_data.app_id = data->app_id;

    init_func(&init_data);

    // Restore the default search order.
    data->SetDllDirectoryA(NULL);
}

// The code that will run in the started process.
// It is responsible of injecting our library into itself.
const u8 REMOTE_FUNC_BYTES[] =
{
    0x55,
    0x8b, 0xec,
    0x83, 0xec, 0x08,
    0x56,
    0x8b, 0x75, 0x08,
    0xff, 0x76, 0x14,
    0x8b, 0x46, 0x08,
    0xff, 0xd0,
    0xff, 0x76, 0x0c,
    0x8b, 0x06,
    0xff, 0xd0,
    0xff, 0x76, 0x10,
    0x8b, 0x4e, 0x04,
    0x50,
    0xff, 0xd1,
    0x8b, 0x4e, 0x14,
    0x89, 0x4d, 0xf8,
    0x8b, 0x4e, 0x18,
    0x89, 0x4d, 0xfc,
    0x8d, 0x4d, 0xf8,
    0x51,
    0xff, 0xd0,
    0x8b, 0x46, 0x08,
    0x83, 0xc4, 0x04,
    0x6a, 0x00,
    0xff, 0xd0,
    0x5e,
    0x8b, 0xe5,
    0x5d,
    0xc2, 0x04, 0x00,
};

// The code that will run in the injected process.
// It is responsible for running REMOTE_FUNC_BYTES.
const u8 REMOTE_THREAD_FUNC_BYTES[] =
{
    0x55,
    0x8b, 0xec,
    0x8b, 0x45, 0x08,
    0xff, 0x30,
    0x8b, 0x40, 0x04,
    0xff, 0xd0,
    0x33, 0xc0,
    0x5d,
    0xc2, 0x04, 0x00
};

// Just call the already generated function in this thread to initialize everything else the same.
DWORD WINAPI remote_thread_func(LPVOID param)
{
    IpcInjectStructure* data = (IpcInjectStructure*)param;
    data->func((ULONG_PTR)data->param);
    return 0;
}

const s32 FULL_ARGS_SIZE = 512;

bool append_custom_launch_params(SupportedGame* game, char* out_buf)
{
    SvrIniMem ini_mem;

    if (!svr_open_ini_read("svr_launch_params.ini", &ini_mem))
    {
        return false;
    }

    SvrIniLine ini_line = svr_alloc_ini_line();
    SvrIniTokenType ini_token_type;

    char buf[64];
    StringCchPrintfA(buf, 64, "%u", game->app_id);

    while (svr_read_ini(&ini_mem, &ini_line, &ini_token_type))
    {
        switch (ini_token_type)
        {
            case SVR_INI_KV:
            {
                if (!strcmp(buf, ini_line.title))
                {
                    if (strlen(ini_line.value) > 0)
                    {
                        StringCchCatA(out_buf, FULL_ARGS_SIZE, " ");
                        StringCchCatA(out_buf, FULL_ARGS_SIZE, ini_line.value);
                    }

                    return true;
                }

                break;
            }
        }
    }

    return false;
}

// Use the launch parameters from Steam if we can.
bool append_steam_launch_params(SupportedGame* game, char* out_buf)
{
    char full_vdf_path[MAX_PATH];
    full_vdf_path[0] = 0;

    char buf[64];
    StringCchPrintfA(buf, 64, "%lu", steam_active_user);

    StringCchCatA(full_vdf_path, MAX_PATH, steam_path);
    StringCchCatA(full_vdf_path, MAX_PATH, "\\userdata\\");
    StringCchCatA(full_vdf_path, MAX_PATH, buf);
    StringCchCatA(full_vdf_path, MAX_PATH, "\\config\\localconfig.vdf");

    SvrVdfMem vdf_mem;

    if (!svr_open_vdf_read(full_vdf_path, &vdf_mem))
    {
        // Steam must be installed wrong if this fails.
        launcher_log("Could not read Steam user settings. Steam may be installed wrong.");
        return false;
    }

    SvrVdfLine vdf_line = svr_alloc_vdf_line();
    SvrVdfTokenType vdf_token_type;

    s32 depth = 0;

    StringCchPrintfA(buf, 64, "%u", game->app_id);

    while (svr_read_vdf(&vdf_mem, &vdf_line, &vdf_token_type))
    {
        switch (vdf_token_type)
        {
            case SVR_VDF_GROUP_TITLE:
            {
                if (depth == 0 && !strcmpi(vdf_line.title, "UserLocalConfigStore")) { depth++; break; }
                else if (depth == 1 && !strcmpi(vdf_line.title, "Software")) { depth++; break; }
                else if (depth == 2 && !strcmpi(vdf_line.title, "Valve")) { depth++; break; }
                else if (depth == 3 && !strcmpi(vdf_line.title, "Steam")) { depth++; break; }
                else if (depth == 4 && !strcmpi(vdf_line.title, "Apps")) { depth++; break; }
                else if (depth == 5 && !strcmpi(vdf_line.title, buf)) { depth++; break; }
            }

            case SVR_VDF_KV:
            {
                if (depth == 6 && !strcmpi(vdf_line.title, "LaunchOptions"))
                {
                    StringCchCatA(out_buf, FULL_ARGS_SIZE, " ");
                    StringCchCatA(out_buf, FULL_ARGS_SIZE, vdf_line.value);
                    return true;
                }

                break;
            }
        }
    }

    launcher_log("Steam launch parameters for %s could not be found\n", game->name);
    return false;
}

void find_game_paths(SupportedGame* game, char* game_path, char* acf_path)
{
    char buf[64];
    StringCchPrintfA(buf, 64, "%u", game->app_id);

    for (s32 i = 0; i < num_steam_libraries; i++)
    {
        FilePathA& lib = steam_library_paths[i];

        // If this file is available here then the game is installed in this library.

        char id_file_path[MAX_PATH];
        id_file_path[0] = 0;
        StringCchCatA(id_file_path, MAX_PATH, lib.path);
        StringCchCatA(id_file_path, MAX_PATH, "appmanifest_");
        StringCchCatA(id_file_path, MAX_PATH, buf);
        StringCchCatA(id_file_path, MAX_PATH, ".acf");

        WIN32_FILE_ATTRIBUTE_DATA attr;

        if (GetFileAttributesExA(id_file_path, GetFileExInfoStandard, &attr))
        {
            StringCchCatA(game_path, MAX_PATH, lib.path);
            StringCchCatA(game_path, MAX_PATH, game->root_dir);

            StringCchCopyA(acf_path, MAX_PATH, id_file_path);
            return;
        }
    }

    // Cannot happen unless Steam is installed wrong in which case it wouldn't work anyway.
    launcher_error("Game %s was not found in any Steam library. Steam may be installed wrong.", game->name);
}

void find_game_build(SupportedGame* game, const char* acf_path, s32* build_id)
{
    SvrVdfMem vdf_mem;

    if (!svr_open_vdf_read(acf_path, &vdf_mem))
    {
        launcher_log("Could not open appmanifest of game %s (%lu)\n", game->name, GetLastError());
        return;
    }

    SvrVdfLine vdf_line = svr_alloc_vdf_line();
    SvrVdfTokenType vdf_token_type;

    s32 depth = 0;

    while (svr_read_vdf(&vdf_mem, &vdf_line, &vdf_token_type))
    {
        switch (vdf_token_type)
        {
            case SVR_VDF_GROUP_TITLE:
            {
                if (depth == 0 && !strcmpi(vdf_line.title, "AppState")) { depth++; break; }
            }

            case SVR_VDF_KV:
            {
                // Value will be like "buildid" "7421361".

                if (depth == 1)
                {
                    if (!strcmpi(vdf_line.title, "buildid"))
                    {
                        *build_id = strtol(vdf_line.value, NULL, 10);
                        return;
                    }
                }

                break;
            }
        }
    }

    svr_log("Build number was not found in appmanifest of game %s\n", game->name);
}

void test_game_build_against_known(SupportedGame* game, s32 build_id)
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

void allocate_in_remote_process(SupportedGame* game, HANDLE process, void** remote_func_addr, void** remote_structure_addr)
{
    // Allocate a sufficient enough size in the target process.
    // It needs to be able to contain all function bytes and the structure containing variable length strings.
    // The virtual memory that we allocated should not be freed as it will be used
    // as reference for future use within the application itself.
    void* remote_mem = VirtualAllocEx(process, NULL, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (remote_mem == NULL)
    {
        DWORD code = GetLastError();
        TerminateProcess(process, 1);
        svr_log("VirtualAllocEx failed with code %lu\n", code);
        launcher_error("Could not initialize standalone SVR. If you use an antivirus, add exception or disable.");
    }

    SIZE_T remote_write_pos = 0;
    SIZE_T remote_written = 0;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, REMOTE_FUNC_BYTES, sizeof(REMOTE_FUNC_BYTES), &remote_written);
    *remote_func_addr = (void*)((char*)remote_mem + remote_write_pos);
    remote_write_pos += remote_written;

    // All addresses here must match up in the context of the target process, not our own.
    // The operating system api functions will always be located in the same address of every
    // process so those do not have to be adjusted.
    IpcStructure structure;
    IpcInjectStructure inject_structure;

    structure.LoadLibraryA = LoadLibraryA;
    structure.GetProcAddress = GetProcAddress;
    structure.SetDllDirectoryA = SetDllDirectoryA;

    const char* game_dll_name = "svr_game.dll";
    const char* init_fn_name = "svr_init_from_launcher";

    #ifdef SVR_INJECTOR
    init_fn_name = "svr_init_from_injector";
    #endif

    char full_dll_path[MAX_PATH];
    full_dll_path[0] = 0;
    StringCchCatA(full_dll_path, MAX_PATH, working_dir);
    StringCchCatA(full_dll_path, MAX_PATH, "\\");
    StringCchCatA(full_dll_path, MAX_PATH, game_dll_name);

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, full_dll_path, strlen(full_dll_path) + 1, &remote_written);
    structure.library_name = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, init_fn_name, strlen(init_fn_name) + 1, &remote_written);
    structure.export_name = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, working_dir, strlen(working_dir) + 1, &remote_written);
    structure.svr_path = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    structure.app_id = game->app_id;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, &structure, sizeof(IpcStructure), &remote_written);
    *remote_structure_addr = (void*)((char*)remote_mem + remote_write_pos);
    remote_write_pos += remote_written;

    // Launcher and injector uses different functions and parameters.

    #if SVR_INJECTOR
    inject_structure.func = (PAPCFUNC)*remote_func_addr;
    inject_structure.param = (IpcStructure*)*remote_structure_addr;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, &inject_structure, sizeof(IpcStructure), &remote_written);
    *remote_structure_addr = (void*)((char*)remote_mem + remote_write_pos); // Overwrite structure address.
    remote_write_pos += remote_written;

    WriteProcessMemory(process, (char*)remote_mem + remote_write_pos, REMOTE_THREAD_FUNC_BYTES, sizeof(REMOTE_THREAD_FUNC_BYTES), &remote_written);
    *remote_func_addr = (void*)((char*)remote_mem + remote_write_pos); // Overwrite function address.
    remote_write_pos += remote_written;
    #endif
}

s32 start_game(SupportedGame* game)
{
    // We don't need the game directory necessarily (mods work differently) since we apply the -game parameter.
    // All known Source games will use SetCurrentDirectory to the mod (game) directory anyway.

    char full_exe_path[MAX_PATH];
    full_exe_path[0] = 0;

    char installed_game_path[MAX_PATH];
    installed_game_path[0] = 0;

    char game_acf_path[MAX_PATH];
    game_acf_path[0] = 0;

    find_game_paths(game, installed_game_path, game_acf_path);

    s32 game_build_id = 0;
    find_game_build(game, game_acf_path, &game_build_id);

    StringCchCatA(full_exe_path, MAX_PATH, installed_game_path);
    StringCchCatA(full_exe_path, MAX_PATH, game->exe_name);

    char full_args[FULL_ARGS_SIZE];
    full_args[0] = 0;

    StringCchCatA(full_args, FULL_ARGS_SIZE, BASE_GAME_ARGS);

    // Prioritize custom args over Steam.

    if (!append_custom_launch_params(game, full_args))
    {
        append_steam_launch_params(game, full_args);
    }

    // Use the written game arg if the user params don't specify it.

    const char* custom_game_arg = strstr(full_args, "-game ");

    if (custom_game_arg == NULL)
    {
        StringCchCatA(full_args, FULL_ARGS_SIZE, " ");
        StringCchCatA(full_args, FULL_ARGS_SIZE, game->extra_args);
    }

    else
    {
        char game_dir[MAX_PATH];
        s32 used = sscanf_s(custom_game_arg, "%*s %s", game_dir, MAX_PATH - 1);

        if (used == 1)
        {
            launcher_log("Using %s as custom game override\n", game_dir);
        }

        else
        {
            launcher_error("Custom -game parameter in Steam or svr_launch_params.ini is missing the value. The value should be the mod name.");
        }
    }

    test_game_build_against_known(game, game_build_id);

    launcher_log("Starting %s (build %d). If launching doesn't work then make sure any antivirus is disabled\n", game->name, game_build_id);

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION info;

    if (!CreateProcessA(full_exe_path, full_args, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &start_info, &info))
    {
        svr_log("CreateProcessA failed with code %lu\n", GetLastError());
        launcher_error("Could not initialize standalone SVR. If you use an antivirus, add exception or disable.");
    }

    void* remote_func_addr;
    void* remote_structure_addr;
    allocate_in_remote_process(game, info.hProcess, &remote_func_addr, &remote_structure_addr);

    // Queue up our procedural function to run instantly on the main thread when the process is resumed.
    if (!QueueUserAPC((PAPCFUNC)remote_func_addr, info.hThread, (ULONG_PTR)remote_structure_addr))
    {
        DWORD code = GetLastError();
        TerminateProcess(info.hProcess, 1);
        svr_log("QueueUserAPC failed with code %lu\n", code);
        launcher_error("Could not initialize standalone SVR. If you use an antivirus, add exception or disable.");
    }

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

s32 inject_game(SupportedGame* game, HANDLE game_proc)
{
    // When injecting we don't have to deal with argument appending stuff.

    char full_exe_path[MAX_PATH];
    full_exe_path[0] = 0;

    char installed_game_path[MAX_PATH];
    installed_game_path[0] = 0;

    char game_acf_path[MAX_PATH];
    game_acf_path[0] = 0;

    find_game_paths(game, installed_game_path, game_acf_path);

    s32 game_build_id = 0;
    find_game_build(game, game_acf_path, &game_build_id);

    test_game_build_against_known(game, game_build_id);

    launcher_log("Injecting into %s (build %d). If injecting doesn't work then make sure any antivirus is disabled\n", game->name, game_build_id);

    void* remote_func_addr;
    void* remote_structure_addr;
    allocate_in_remote_process(game, game_proc, &remote_func_addr, &remote_structure_addr);

    HANDLE remote_thread = CreateRemoteThread(game_proc, NULL, 0, (LPTHREAD_START_ROUTINE)remote_func_addr, remote_structure_addr, CREATE_SUSPENDED, NULL);

    if (remote_thread == NULL)
    {
        svr_log("CreateRemoteThread failed with code %lu\n", GetLastError());
        launcher_error("Could not inject into game. If you use an antivirus, add exception or disable.");
    }

    svr_log("Injector finished, rest of the log is from the game\n");
    svr_log("---------------------------------------------------\n");

    // Need to close the file so the game can open it.
    svr_shutdown_log();

    // Let the injection actually start now.
    // You want to place a breakpoint on this line when debugging the game!
    // When this breakpoint is hit, attach to the game process and then continue this process.
    ResumeThread(remote_thread);

    CloseHandle(remote_thread);

    // We don't have to wait here since we don't print to the launcher console from the game anymore.
    // WaitForSingleObject(game_proc, INFINITE);

    CloseHandle(game_proc);

    return 0;
}

s32 get_choice_from_user(s32 min, s32 max)
{
    s32 selection = -1;

    while (selection == -1)
    {
        char buf[4];
        char* res = fgets(buf, 4, stdin);

        if (res == NULL)
        {
            // Can get here from Ctrl+C.
            return -1;
        }

        selection = strtol(buf, NULL, 10);

        if (selection <= min || selection > max)
        {
            selection = -1;
            continue;
        }
    }

    return selection - 1; // Numbers displayed are 1 based.
}

s32 show_menu_for_launcher()
{
    launcher_log("Installed games:\n");

    s32 j = 0;

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        SupportedGame* game = &SUPPORTED_GAMES[i];

        // Show indexes for the installed games, not the supported games.
        if (steam_install_states[i])
        {
            launcher_log("[%d] %s\n", j + 1, game->name);
            j++;
        }
    }

    printf("\nSelect which game to start: ");

    s32 max_usable_games = j;
    s32 selection = get_choice_from_user(0, max_usable_games);

    if (selection < 0)
    {
        return 1;
    }

    // Need to remap from the installed games to the supported games.
    s32 game_index = steam_index_remaps[selection];

    SupportedGame* game = &SUPPORTED_GAMES[game_index];

    return start_game(game);
}

s32 show_menu_for_injector()
{
    launcher_log("Running games:\n");

    s32 j = 0;
    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        SupportedGame* game = &SUPPORTED_GAMES[i];

        // Show indexes for the installed games, not the supported games.
        if (running_games[i])
        {
            launcher_log("[%d] %s\n", j + 1, game->name);
            j++;
        }
    }

    printf("\nSelect which game to inject to: ");

    s32 max_usable_games = j;
    s32 selection = get_choice_from_user(0, max_usable_games);

    if (selection < 0)
    {
        return 1;
    }

    // Need to remap from the installed games to the supported games.
    s32 game_index = running_index_remaps[selection];

    SupportedGame* game = &SUPPORTED_GAMES[game_index];
    HANDLE game_proc = running_procs[game_index];

    return inject_game(game, game_proc);
}

s32 autostart_game(SteamAppId app_id)
{
    s32 game_index = -1;

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        SupportedGame* game = &SUPPORTED_GAMES[i];

        if (steam_install_states[i])
        {
            if (app_id == game->app_id)
            {
                game_index = i;
                break;
            }
        }
    }

    if (game_index == -1)
    {
        // Show why this game cannot be autostarted.

        s32 flags = 0;

        const s32 SUPPORTED = 1 << 0;
        const s32 INSTALLED = 1 << 1;

        for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
        {
            SupportedGame* game = &SUPPORTED_GAMES[i];

            if (app_id == game->app_id)
            {
                flags |= SUPPORTED;

                if (steam_install_states[i])
                {
                    flags |= INSTALLED;
                }

                break;
            }
        }

        if (!(flags & SUPPORTED))
        {
            launcher_error("Cannot autostart, app id %u is not supported.", app_id);
        }

        else
        {
            if (!(flags & INSTALLED))
            {
                launcher_error("Cannot autostart, app id %u is not installed.", app_id);
            }
        }
    }

    SupportedGame* game = &SUPPORTED_GAMES[game_index];

    return start_game(game);
}

void show_windows_version()
{
    HKEY hkey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hkey) != 0)
    {
        return;
    }

    const s32 REG_BUF_SIZE = 64;

    char product_name[REG_BUF_SIZE];
    char current_build[REG_BUF_SIZE];
    char release_id[REG_BUF_SIZE];

    DWORD product_name_size = REG_BUF_SIZE;
    DWORD current_build_size = REG_BUF_SIZE;
    DWORD release_id_size = REG_BUF_SIZE;

    RegGetValueA(hkey, NULL, "ProductName", RRF_RT_REG_SZ, NULL, product_name, &product_name_size);
    RegGetValueA(hkey, NULL, "CurrentBuild", RRF_RT_REG_SZ, NULL, current_build, &current_build_size);
    RegGetValueA(hkey, NULL, "ReleaseId", RRF_RT_REG_SZ, NULL, release_id, &release_id_size);

    // Will show like Windows 10 Enterprise version 2004 build 19041.
    char winver[192];
    StringCchPrintfA(winver, 192, "%s version %d build %d", product_name, strtol(release_id, NULL, 10), strtol(current_build, NULL, 10));

    svr_log("Using operating system %s\n", winver);
}

bool is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

void trim_right(char* buf, s32 length)
{
    s32 len = length;
    char* start = buf;
    char* end = buf + len - 1;

    while (end != start && is_whitespace(*end))
    {
        end--;
    }

    end++;
    *end = 0;
}

void show_processor()
{
    HKEY hkey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hkey) != 0)
    {
        return;
    }

    const s32 REG_BUF_SIZE = 128;

    char name[REG_BUF_SIZE];
    DWORD name_size = REG_BUF_SIZE;

    RegGetValueA(hkey, NULL, "ProcessorNameString", RRF_RT_REG_SZ, NULL, name, &name_size);

    // The value will have a lot of extra spaces at the end.
    trim_right(name, strlen(name));

    svr_log("Using processor %s (%lu cpus)\n", name, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
}

void show_available_memory()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);

    GlobalMemoryStatusEx(&mem);

    svr_log("The system has %lld mb of memory (%lld mb usable)\n", SVR_FROM_MB(mem.ullTotalPhys), SVR_FROM_MB(mem.ullAvailPhys));
}

void find_steam_path()
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

// For backslashes only, makes \\ into \ (or \\\\ into \\).
void unescape_path(char* buf, char* dest)
{
    char* ptr = buf;
    s32 i = 0;

    for (; *ptr != 0; ptr++)
    {
        if (*ptr == '\\' && *(ptr + 1) == '\\')
        {
            continue;
        }

        dest[i] = *ptr;
        i++;
    }

    dest[i] = 0;
}

void find_steam_libraries()
{
    // The installation path is always a default library path, so add that first.

    char full_def_path[MAX_PATH];
    full_def_path[0] = 0;
    StringCchCatA(full_def_path, MAX_PATH, steam_path);
    StringCchCatA(full_def_path, MAX_PATH, "\\steamapps\\");

    StringCchCopyA(steam_library_paths[num_steam_libraries].path, MAX_PATH, full_def_path);
    num_steam_libraries++;

    char full_vdf_path[MAX_PATH];
    full_vdf_path[0] = 0;
    StringCchCatA(full_vdf_path, MAX_PATH, steam_path);
    StringCchCatA(full_vdf_path, MAX_PATH, "\\steamapps\\libraryfolders.vdf");

    SvrVdfMem vdf_mem;

    if (!svr_open_vdf_read(full_vdf_path, &vdf_mem))
    {
        // Not having any extra Steam libraries is ok.
        return;
    }

    SvrVdfLine vdf_line = svr_alloc_vdf_line();
    SvrVdfTokenType vdf_token_type;

    s32 depth = 0;

    char cur_lib_number[64];
    StringCchPrintfA(cur_lib_number, 64, "%d", num_steam_libraries);

    while (svr_read_vdf(&vdf_mem, &vdf_line, &vdf_token_type))
    {
        switch (vdf_token_type)
        {
            case SVR_VDF_GROUP_TITLE:
            {
                if (depth == 0 && !strcmpi(vdf_line.title, "LibraryFolders")) { depth++; break; }
                if (depth == 1 && !strcmpi(vdf_line.title, cur_lib_number)) { depth++; break; }
            }

            case SVR_VDF_KV:
            {
                if (depth == 2)
                {
                    if (!strcmpi(vdf_line.title, "path"))
                    {
                        // Paths in vdf will be escaped, we need to unescape.

                        char new_path[MAX_PATH];
                        new_path[0] = 0;

                        unescape_path(vdf_line.value, new_path);

                        StringCchCatA(new_path, MAX_PATH, "\\steamapps\\");

                        StringCchCopyA(steam_library_paths[num_steam_libraries].path, MAX_PATH, new_path);
                        num_steam_libraries++;

                        if (num_steam_libraries == MAX_STEAM_LIBRARIES)
                        {
                            svr_log("Too many Steam libraries installed, using first %d\n", MAX_STEAM_LIBRARIES);
                            return;
                        }

                        StringCchPrintfA(cur_lib_number, 64, "%d", num_steam_libraries);

                        depth--;
                    }
                }

                break;
            }
        }
    }
}

void find_installed_supported_games()
{
    // We do a quick search through the registry to determine if we have the supported games.

    HKEY steam_apps_hkey;
    RegOpenKeyExA(steam_hkey, "Apps", 0, KEY_READ, &steam_apps_hkey);

    num_installed_games = 0;

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        SupportedGame* game = &SUPPORTED_GAMES[i];

        char buf[64];
        StringCchPrintfA(buf, 64, "%u", game->app_id);

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

        // Remap indexes for the available games.

        steam_install_states[i] = true;
        steam_index_remaps[num_installed_games] = i;
        num_installed_games++;
    }

    if (num_installed_games == 0)
    {
        launcher_error("None of the supported games are installed on Steam.");
    }
}

bool is_process_we_care_about(DWORD pid, s32* the_game_this_is, HANDLE* the_proc)
{
    bool ret = false;
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);

    if (proc == NULL)
    {
        return ret;
    }

    char exe_name[MAX_PATH];
    GetProcessImageFileNameA(proc, exe_name, MAX_PATH);

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        SupportedGame* game = &SUPPORTED_GAMES[i];

        // Not enough to check for the exe, as a lot of games will just have hl2.exe and we need to get uniques.
        if (strstr(exe_name, game->root_dir))
        {
            *the_game_this_is = i;
            *the_proc = proc;
            ret = true;
            break;
        }
    }

    if (!ret)
    {
        CloseHandle(proc);
    }

    return ret;
}

// We use WMI for getting process command lines just because I assume it is safer. You can get this information too by reading the process memory
// but maybe should not do that with VAC potentially running in the process.

IWbemLocator* wbem_locator = NULL;
IWbemServices* wbem_service = NULL;

// Init this once because it is used later for every running game.
void init_crap_wmi_stuff()
{
    CoInitialize(NULL);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wbem_locator));

    wbem_locator->ConnectServer(L"ROOT\\CIMV2", NULL, NULL, 0, NULL, 0, 0, &wbem_service);

    CoSetProxyBlanket(wbem_service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
}

// We only want to be injecting into insecure games.
bool is_game_running_insecure(DWORD pid)
{
    bool ret = false;

    wchar query[256];
    StringCchPrintfW(query, SVR_ARRAY_SIZE(query), L"SELECT CommandLine FROM Win32_Process WHERE ProcessId = %lu", pid);

    IEnumWbemClassObject* enumerator = NULL;
    wbem_service->ExecQuery(L"WQL", query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &enumerator);

    while (true)
    {
        IWbemClassObject* class_obj = NULL;
        ULONG enum_ret = 0;
        enumerator->Next(WBEM_INFINITE, 1, &class_obj, &enum_ret);

        if (enum_ret == 0)
        {
            break;
        }

        VARIANT res_variant;
        VariantInit(&res_variant);

        class_obj->Get(L"CommandLine", 0, &res_variant, 0, 0);

        if (res_variant.vt == VT_BSTR)
        {
            if (wcsstr(res_variant.bstrVal, L"-insecure"))
            {
                ret = true;
            }
        }

        class_obj->Release();

        VariantClear(&res_variant);

        if (ret)
        {
            break;
        }
    }

    enumerator->Release();

    return ret;
}

void find_running_supported_games()
{
    DWORD pids[1024];
    DWORD actual_size = 0;
    EnumProcesses(pids, sizeof(pids), &actual_size);

    s32 num_procs = actual_size / sizeof(DWORD);

    for (s32 i = 0; i < num_procs; i++)
    {
        if (pids[i] == 0)
        {
            continue;
        }

        s32 game_this_is;
        HANDLE game_proc;

        if (!is_process_we_care_about(pids[i], &game_this_is, &game_proc))
        {
            continue;
        }

        if (!is_game_running_insecure(pids[i]))
        {
            CloseHandle(game_proc);
            continue;
        }

        running_games[game_this_is] = true;
        running_procs[game_this_is] = game_proc;
        running_index_remaps[num_installed_games] = game_this_is;
        num_running_games++;
    }

    if (num_running_games == 0)
    {
        launcher_error("There are no supported running games to inject to. Games must be running in insecure mode to be listed here.");
    }
}

// We cannot store this result so it has to be done every start.
void check_hw_caps()
{
    ID3D11Device* d3d11_device = NULL;
    ID3D11DeviceContext* d3d11_context = NULL;

    UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    // Use a lower feature level here than needed (we actually use 12_0) in order to get a better description
    // of the adapter below, and also to more accurately query the hw caps.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_11_0;

    const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
        MINIMUM_DEVICE_LEVEL
    };

    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &d3d11_device, NULL, &d3d11_context);

    if (FAILED(hr))
    {
        svr_log("D3D11CreateDevice failed with code %#x\n", hr);
        launcher_error("HW support could not be queried. Is there a graphics adapter in the system?");
    }

    IDXGIDevice* dxgi_device;
    d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    IDXGIAdapter* dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);

    DXGI_ADAPTER_DESC dxgi_adapter_desc;
    dxgi_adapter->GetDesc(&dxgi_adapter_desc);

    // Useful for future troubleshooting.
    // Use https://www.pcilookup.com/ to see more information about device and vendor ids.
    svr_log("Using graphics device %x by vendor %x\n", dxgi_adapter_desc.DeviceId, dxgi_adapter_desc.VendorId);

    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fmt_support2;
    fmt_support2.InFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &fmt_support2, sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT2));

    bool has_typed_uav_load = fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
    bool has_typed_uav_store = fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
    bool has_typed_uav_support = has_typed_uav_load && has_typed_uav_store;

    if (!has_typed_uav_support)
    {
        launcher_error("This system does not meet the requirements to use SVR.");
    }
}

int main(int argc, char** argv)
{
    // Enable this to generate the machine code for remote_func (see comments at that function).
    #if 0
    IpcStructure structure = {};
    structure.LoadLibraryA = LoadLibraryA;
    structure.GetProcAddress = GetProcAddress;
    structure.SetDllDirectoryA = SetDllDirectoryA;

    // It is important to use QueueUserAPC here to produce the correct output.
    // Calling remote_func directly will produce uniquely optimized code which cannot
    // work in another process.
    QueueUserAPC(remote_func, GetCurrentThread(), (ULONG_PTR)&structure);

    // Used to signal the thread so the queued function will run.
    SleepEx(0, TRUE);

    return 0;
    #endif

    // Enable this to generate the machine code for remote_thread_func (see comments at remote_func).
    #if 0
    CreateThread(NULL, 0, remote_thread_func, NULL, 0, NULL);
    return 0;
    #endif

    GetCurrentDirectoryA(MAX_PATH, working_dir);

    // For standalone mode, the launcher creates the log file that the game then appends to.
    svr_init_log("data\\SVR_LOG.txt", false);

    // Enable to show system information and stuff on start.
    #if 1
    if (!IsWindows10OrGreater())
    {
        launcher_error("Windows 10 or later is needed to use SVR.");
    }

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    launcher_log("SVR version %d (%02d/%02d/%04d %02d:%02d:%02d)\n", SVR_VERSION, lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond);
    launcher_log("This is a standalone version of SVR. Interoperability with other applications may not work\n");
    // launcher_log("To use SVR in Momentum, start Momentum\n");
    launcher_log("For more information see https://github.com/crashfort/SourceDemoRender\n");

    show_windows_version();
    show_processor();
    show_available_memory();
    check_hw_caps();
    #endif

    // We want to find Steam stuff when injecting too so we can get the same information out of the game.

    find_steam_path();
    find_installed_supported_games();
    find_steam_libraries();

    svr_log("Found %d games in %d Steam libraries\n", num_installed_games, num_steam_libraries);

    #ifdef SVR_LAUNCHER
    // We delay finding which Steam library a game is in until a game has been chosen.

    // Autostarting a game works by giving the app id.
    if (argc == 2)
    {
        SteamAppId app_id = strtoul(argv[1], NULL, 10);

        if (app_id == 0)
        {
            return 1;
        }

        return autostart_game(app_id);
    }

    return show_menu_for_launcher();
    #endif

    #ifdef SVR_INJECTOR
    init_crap_wmi_stuff();
    find_running_supported_games();
    return show_menu_for_injector();
    #endif

    return 0;
}
