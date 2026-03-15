#include "game_priv.h"
#include "game_common.h"

void game_hook_init()
{
    MH_STATUS status = MH_Initialize();
    (void)status;
}

void game_hook_create(GameFnOverride* ov, GameFnHook* dest)
{
    // No point handling errors because we cannot validate the result in any way.

    dest->target = ov->target;
    dest->override = ov->override;

    MH_STATUS status = MH_CreateHook(ov->target, ov->override, &dest->original);
    (void)status;
}

void game_hook_api(const char* proc_name, const char* dll, GameFnHook* dest)
{
    HMODULE hmod = GetModuleHandleA(dll);
    FARPROC proc = GetProcAddress(hmod, proc_name);

    dest->target = proc;

    MH_STATUS status = MH_CreateHook(proc, dest->override, &dest->original);
    (void)status;
}

void game_hook_remove(GameFnHook* h)
{
    MH_STATUS status = MH_RemoveHook(h->target);
    (void)status;
}

void game_hook_enable(GameFnHook* h, bool v)
{
    assert(h->target);

    if (v)
    {
        MH_STATUS status = MH_EnableHook(h->target);
        (void)status;
    }

    else
    {
        MH_STATUS status = MH_DisableHook(h->target);
        (void)status;
    }
}

void game_hook_enable_all()
{
    MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
    (void)status;
}
