#include "game_priv.h"

void game_hook_init()
{
    MH_Initialize();
}

void game_hook_create(GameFnOverride* ov, GameFnHook* dest)
{
    // No point handling errors because we cannot validate the result in any way.

    dest->target = ov->target;
    dest->override = ov->override;

    MH_CreateHook(ov->target, ov->override, &dest->original);
}

void game_hook_remove(GameFnHook* h)
{
    MH_RemoveHook(h->target);
}

void game_hook_enable(GameFnHook* h, bool v)
{
    assert(h->target);

    if (v)
    {
        MH_EnableHook(h->target);
    }

    else
    {
        MH_DisableHook(h->target);
    }
}

void game_hook_enable_all()
{
    MH_EnableHook(MH_ALL_HOOKS);
}
