#include "svr_common.h"
#include "svr_defs.h"
#include <Windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "svr_logging.h"
#include <conio.h>
#include "svr_vdf.h"
#include <VersionHelpers.h>
#include "stb_sprintf.h"
#include <d3d11.h>

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

// These are the supported games.
const SteamAppId GAME_APP_IDS[] = {
    STEAM_GAME_CSS,
    STEAM_GAME_CSGO,
};

// Display names shown on Steam.
const char* GAME_NAMES[] = {
    "Counter-Strike: Source",
    "Counter-Strike: Global Offensive",
};

// Extra stuff to put in the start args.
const char* EXTRA_GAME_ARGS[] = {
    "-game cstrike", // Counter-Strike: Source
    "-game csgo", // Counter-Strike: Global Offensive
};

// Paths to append to each Steam library.
const char* GAME_ROOT_DIRS[] = {
    "common\\Counter-Strike Source\\", // Counter-Strike: Source
    "common\\Counter-Strike Global Offensive\\", // Counter-Strike: Global Offensive
};

// Where to find the executable built up from the Steam library path plus the game root directory (above).
const char* GAME_EXE_PATHS[] = {
    "hl2.exe", // Counter-Strike: Source
    "csgo.exe", // Counter-Strike: Global Offensive
};

// Build versions that have been tested (located in the appmanifest acf).
s32 GAME_BUILDS[] = {
    6946501, // Counter-Strike: Source
    7421361, // Counter-Strike: Global Offensive
};

const s32 NUM_SUPPORTED_GAMES = SVR_ARRAY_SIZE(GAME_APP_IDS);

// Base arguments that every game will have.
const char* BASE_GAME_ARGS = "-steam -insecure +sv_lan 1 -console -novid";

const s32 MAX_STEAM_LIBRARIES = 32;

// Whether or not the games we support are installed.
bool steam_install_states[NUM_SUPPORTED_GAMES];
s32 num_installed_games;

// A Steam library can be installed anywhere, we have to iterate over all of them to see where a game is located.

s32 num_steam_libraries;
FilePathA steam_library_paths[MAX_STEAM_LIBRARIES];

// This is used to remap indexes when selecting games in the menu from games that are installed vs games that are supported.
s32 steam_index_remaps[NUM_SUPPORTED_GAMES];

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

    // We need to call svr_init_standalone in svr_game.dll.

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

const s32 FULL_ARGS_SIZE = 512;

// Use the launch parameters from Steam if we can.
bool append_steam_launch_params(s32 game_index, char* out_buf)
{
    char full_vdf_path[MAX_PATH];
    full_vdf_path[0] = 0;

    char buf[64];
    StringCchPrintfA(buf, 64, "%lu", steam_active_user);

    StringCchCatA(full_vdf_path, MAX_PATH, steam_path);
    StringCchCatA(full_vdf_path, MAX_PATH, "\\userdata\\");
    StringCchCatA(full_vdf_path, MAX_PATH, buf);
    StringCchCatA(full_vdf_path, MAX_PATH, "\\config\\localconfig.vdf");

    SvrVdfMem mem;

    if (!svr_open_vdf_read(full_vdf_path, &mem))
    {
        // Steam must be installed wrong if this fails.
        launcher_log("Could not read Steam user settings. Steam may be installed wrong.");
        return false;
    }

    SvrVdfLine vdf_line = svr_alloc_vdf_line();
    SvrVdfTokenType vdf_token_type;

    s32 depth = 0;

    StringCchPrintfA(buf, 64, "%u", GAME_APP_IDS[game_index]);

    while (svr_read_vdf(&mem, &vdf_line, &vdf_token_type))
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

    launcher_log("Steam launch parameters for %s could not be found\n", GAME_NAMES[game_index]);
    return false;
}

void find_game_paths(s32 game_index, char* game_path, char* acf_path)
{
    char buf[64];
    StringCchPrintfA(buf, 64, "%u", GAME_APP_IDS[game_index]);

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
            StringCchCatA(game_path, MAX_PATH, GAME_ROOT_DIRS[game_index]);

            StringCchCopyA(acf_path, MAX_PATH, id_file_path);
            return;
        }
    }

    // Cannot happen unless Steam is installed wrong in which case it wouldn't work anyway.
    launcher_error("Game %s was not found in any Steam library. Steam may be installed wrong.", GAME_NAMES[game_index]);
}

void find_game_build(s32 game_index, const char* acf_path, s32* build_id)
{
    SvrVdfMem vdf_mem;

    if (!svr_open_vdf_read(acf_path, &vdf_mem))
    {
        launcher_log("Could not open appmanifest of game %s (%lu)\n", GAME_NAMES[game_index], GetLastError());
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

    launcher_log("Build number was not found in appmanifest of game %s\n", GAME_NAMES[game_index]);
}

s32 start_game(s32 game_index)
{
    // We don't need the game directory necessarily (mods work differently) since we apply the -game parameter.
    // All known Source games will use SetCurrentDirectory to the mod (game) directory anyway.

    char full_exe_path[MAX_PATH];
    full_exe_path[0] = 0;

    char installed_game_path[MAX_PATH];
    installed_game_path[0] = 0;

    char game_acf_path[MAX_PATH];
    game_acf_path[0] = 0;

    find_game_paths(game_index, installed_game_path, game_acf_path);

    s32 game_build_id = 0;
    find_game_build(game_index, game_acf_path, &game_build_id);

    StringCchCatA(full_exe_path, MAX_PATH, installed_game_path);
    StringCchCatA(full_exe_path, MAX_PATH, GAME_EXE_PATHS[game_index]);

    char full_args[FULL_ARGS_SIZE];
    full_args[0] = 0;

    StringCchCatA(full_args, FULL_ARGS_SIZE, BASE_GAME_ARGS);
    StringCchCatA(full_args, FULL_ARGS_SIZE, " ");
    StringCchCatA(full_args, FULL_ARGS_SIZE, EXTRA_GAME_ARGS[game_index]);

    append_steam_launch_params(game_index, full_args);

    if (game_build_id > 0)
    {
        s32 tested_build_id = GAME_BUILDS[game_index];

        if (game_build_id != tested_build_id)
        {
            launcher_log("WARNING: Mismatch between installed Steam build and tested SVR build. "
                         "Steam game build is %d and tested build is %d. "
                         "Problems may occur as this game build has not been tested!\n", game_build_id, tested_build_id);
        }
    }

    launcher_log("Starting %s (build %d). If launching doesn't work then make sure any antivirus is disabled\n", GAME_NAMES[game_index], game_build_id);

    STARTUPINFOA start_info = {};
    start_info.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION info;

    if (!CreateProcessA(full_exe_path, full_args, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &start_info, &info))
    {
        launcher_error("Could not start game (code %lu). If you use an antivirus, add exception or disable.", GetLastError());
    }

    // Allocate a sufficient enough size in the target process.
    // It needs to be able to contain all function bytes and the structure containing variable length strings.
    // The virtual memory that we allocated should not be freed as it will be used
    // as reference for future use within the application itself.
    void* remote_mem = VirtualAllocEx(info.hProcess, NULL, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (remote_mem == NULL)
    {
        TerminateProcess(info.hProcess, 1);
        launcher_error("Could not initialize standalone SVR (code %lu). If you use an antivirus, add exception or disable.", GetLastError());
    }

    SIZE_T remote_write_pos = 0;
    SIZE_T remote_written = 0;

    WriteProcessMemory(info.hProcess, (char*)remote_mem + remote_write_pos, REMOTE_FUNC_BYTES, sizeof(REMOTE_FUNC_BYTES), &remote_written);
    void* remote_func_addr = (void*)((char*)remote_mem + remote_write_pos);
    remote_write_pos += remote_written;

    // All addresses here must match up in the context of the target process, not our own.
    // The operating system api functions will always be located in the same address of every
    // process so those do not have to be adjusted.
    IpcStructure structure;

    structure.LoadLibraryA = LoadLibraryA;
    structure.GetProcAddress = GetProcAddress;
    structure.SetDllDirectoryA = SetDllDirectoryA;

    char GAME_DLL_NAME[] = "svr_game.dll";
    char INIT_FN_NAME[] = "svr_init_standalone";

    WriteProcessMemory(info.hProcess, (char*)remote_mem + remote_write_pos, GAME_DLL_NAME, SVR_ARRAY_SIZE(GAME_DLL_NAME), &remote_written);
    structure.library_name = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    WriteProcessMemory(info.hProcess, (char*)remote_mem + remote_write_pos, INIT_FN_NAME, SVR_ARRAY_SIZE(INIT_FN_NAME), &remote_written);
    structure.export_name = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    WriteProcessMemory(info.hProcess, (char*)remote_mem + remote_write_pos, working_dir, strlen(working_dir) + 1, &remote_written);
    structure.svr_path = (char*)remote_mem + remote_write_pos;
    remote_write_pos += remote_written;

    structure.app_id = GAME_APP_IDS[game_index];

    WriteProcessMemory(info.hProcess, (char*)remote_mem + remote_write_pos, &structure, sizeof(IpcStructure), &remote_written);
    void* remote_structure_addr = (void*)((char*)remote_mem + remote_write_pos);
    remote_write_pos += remote_written;

    // Queue up our procedural function to run instantly on the main thread when the process is resumed.
    if (!QueueUserAPC((PAPCFUNC)remote_func_addr, info.hThread, (ULONG_PTR)remote_structure_addr))
    {
        TerminateProcess(info.hProcess, 1);
        launcher_error("Could not initialize standalone SVR (code %lu). If you use an antivirus, add exception or disable.", GetLastError());
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

s32 show_menu()
{
    launcher_log("Installed games:\n");

    s32 j = 0;
    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        // Show indexes for the installed games, not the supported games.
        if (steam_install_states[i])
        {
            launcher_log("[%d] %s\n", j + 1, GAME_NAMES[i]);
            j++;
        }
    }

    s32 max_usable_games = j;
    s32 selection = -1;

    printf("\nSelect which game to start: ");

    while (selection == -1)
    {
        char buf[4];
        char* res = fgets(buf, 4, stdin);

        if (res == NULL)
        {
            // Can get here from Ctrl+C.
            return 1;
        }

        selection = strtol(buf, NULL, 10);

        if (selection <= 0 || selection > max_usable_games)
        {
            selection = -1;
            continue;
        }
    }

    // Need to remap from the installed games to the supported games.
    s32 game_index = steam_index_remaps[selection - 1];

    return start_game(game_index);
}

s32 autostart_game(SteamAppId app_id)
{
    s32 game_index = -1;

    for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
    {
        if (steam_install_states[i])
        {
            if (app_id == GAME_APP_IDS[i])
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
            if (app_id == GAME_APP_IDS[i])
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

    return start_game(game_index);
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

    launcher_log("Using operating system %s\n", winver);
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

    launcher_log("Using processor %s (%lu cpus)\n", name, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
}

void show_available_memory()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);

    GlobalMemoryStatusEx(&mem);

    launcher_log("The system has %lld mb of memory (%lld mb usable)\n", SVR_FROM_MB(mem.ullTotalPhys), SVR_FROM_MB(mem.ullAvailPhys));
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

    while (svr_read_vdf(&vdf_mem, &vdf_line, &vdf_token_type))
    {
        switch (vdf_token_type)
        {
            case SVR_VDF_GROUP_TITLE:
            {
                if (depth == 0 && !strcmpi(vdf_line.title, "LibraryFolders")) { depth++; break; }
            }

            case SVR_VDF_KV:
            {
                // Value will be like "1" "B:\\SteamLibrary" with an increasing number for each library.

                if (depth == 1)
                {
                    char buf[64];
                    StringCchPrintfA(buf, 64, "%d", num_steam_libraries);

                    if (!strcmpi(vdf_line.title, buf))
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
                            launcher_log("Too many Steam libraries installed, using first %d\n", MAX_STEAM_LIBRARIES);
                            return;
                        }
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
        char buf[64];
        StringCchPrintfA(buf, 64, "%u", GAME_APP_IDS[i]);

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
        char message[1024];
        message[0] = 0;
        StringCchCatA(message, 1024, "None of the supported games are installed on Steam. These are the supported games:\n\n");

        for (s32 i = 0; i < NUM_SUPPORTED_GAMES; i++)
        {
            StringCchCatA(message, 1024, GAME_NAMES[i]);

            if (i != NUM_SUPPORTED_GAMES)
            {
                StringCchCatA(message, 1024, "\n");
            }
        }

        launcher_error(message);
    }
}

// We cannot store this result so it has to be done every start.
void check_hw_caps()
{
    ID3D11Device* d3d11_device = NULL;
    ID3D11DeviceContext* d3d11_context = NULL;

    UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    // Should be good enough for all the features that we make use of.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_11_0;

    const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
        MINIMUM_DEVICE_LEVEL
    };

    D3D_FEATURE_LEVEL created_device_level;

    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &d3d11_device, &created_device_level, &d3d11_context);

    if (FAILED(hr))
    {
        launcher_error("HW support could not be queried (%#x). Is there a graphics adapter in the system?", hr);
    }

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

    GetCurrentDirectoryA(MAX_PATH, working_dir);

    // For standalone mode, the launcher creates the log file that the game then appends to.
    svr_init_log("data\\SVR_LOG.TXT", false);

    // Enable to show system information and stuff on start.
    #if 1
    if (!IsWindows10OrGreater())
    {
        launcher_error("Windows 10 or later is needed to use SVR.");
    }

    check_hw_caps();

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    launcher_log("SVR version %d (%02d/%02d/%04d %02d:%02d:%02d)\n", SVR_VERSION, lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond);
    launcher_log("This is a standalone version of SVR. Interoperability with other applications may not work\n");
    // launcher_log("To use SVR in Momentum, start Momentum\n");
    launcher_log("For more information see https://github.com/crashfort/SourceDemoRender\n");

    show_windows_version();
    show_processor();
    show_available_memory();
    #endif

    find_steam_path();
    find_installed_supported_games();
    find_steam_libraries();

    svr_log("Found %d games in %d Steam libraries\n", num_installed_games, num_steam_libraries);

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

    return show_menu();
}
