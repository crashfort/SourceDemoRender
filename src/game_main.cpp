#include "game_shared.h"
#include <Windows.h>
#include <MinHook.h>
#include <assert.h>
#include "svr_prof.h"

// We don't have to verify that we actually are the game that we get passed in, as it is impossible to fabricate
// these variables externally without modifying the launcher code.
// If our code even runs in here then the game already exist and we don't have to deal with weird cases.

const char* svr_resource_path;
u32 game_app_id;

bool setup_wait_for_libs();
bool setup_create_hooks();
bool setup_finalize();

DWORD WINAPI init_async(void* param)
{
    // We don't want to show message boxes on failures because they work real bad in fullscreen.
    // Need to come up with some other mechanism.

    svr_game_init_log(svr_resource_path);

    // We append to the log the launcher created so notify that we have started.
    svr_log("Hello from the game\n");

    if (!setup_wait_for_libs())
    {
        return 1;
    }

    MH_Initialize();

    if (!setup_create_hooks())
    {
        return 1;
    }

    // All threads will be frozen during this period.
    MH_EnableHook(MH_ALL_HOOKS);

    if (!setup_finalize())
    {
        return 1;
    }

    svr_init_prof();

    return 0;
}

// Called when launching. This is before the process has started, and there are no game libraries loaded here.
// This will get called when we have runtime injection too.
// When injecting you can read HKEY_CURRENT_USER\SOFTWARE\Valve\Steam\RunningAppID to see which game id is running.
extern "C" __declspec(dllexport) void svr_init(SvrGameInitData* init_data)
{
    svr_resource_path = init_data->svr_path;
    game_app_id = init_data->app_id;

    // Init needs to be done async because we need to wait for the libraries to load while the game loads as normal.
    CreateThread(NULL, 0, init_async, NULL, 0, NULL);
}
