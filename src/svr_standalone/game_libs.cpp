#include "game_priv.h"

// Library waiting.

s32 game_check_loaded_proc_modules(const char** list, s32 size)
{
    s32 hits = 0;

    // See if all of the requested modules are loaded.

    for (s32 i = 0; i < size; i++)
    {
        // This function does not care about character case.
        HMODULE module = GetModuleHandleA(list[i]); // Does not increment the reference count of the module.

        if (module)
        {
            hits++;
        }
    }

    return hits;
}

bool game_wait_for_libs_to_load(const char** libs, s32 num, s32 timeout)
{
    // Alternate method instead of hooking the LoadLibrary family of functions.
    // We don't need accuracy so this is good enough and much simpler.

    s32 num_loops = timeout * 10;

    for (s32 i = 0; i < num_loops; i++)
    {
        if (game_check_loaded_proc_modules(libs, num) == num)
        {
            return true;
        }

        Sleep(100);
    }

    return false;
}
