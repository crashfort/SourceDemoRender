#include "game_priv.h"
#include "game_common.h"

#define GAME_CALL_PROXY(PX, PARAM, RES) (PX).proxy(&(PX), (PARAM), (RES))

void game_spec_target_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = s32(__cdecl*)();
    DestFn fn = (DestFn)proxy->target;
    *(s32*)res = fn();
}

// ----------------------------------------------------------------

void game_engine_client_command_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = void(__fastcall*)(void* p, void* edx, const char* str);
    DestFn fn = (DestFn)proxy->target;
    fn(NULL, NULL, (const char*)params);
}

void game_engine_client_command_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = void(__fastcall*)(void* p, const char* str);
    DestFn fn = (DestFn)proxy->target;
    fn(NULL, (const char*)params);
}

// ----------------------------------------------------------------

void game_player_by_index_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = void*(__cdecl*)(s32 index);
    DestFn fn = (DestFn)proxy->target;
    *(void**)res = fn(*(s32*)params);
}

// ----------------------------------------------------------------

void game_velocity_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    u8* ptr = (u8*)params;
    ptr += (s32)proxy->target;
    memcpy(res, ptr, sizeof(SvrVec3));
}

void game_buttons_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    u8* ptr = (u8*)params;
    ptr += (s32)proxy->target;
    *(u32*)res = *(s32*)ptr;
}

// ----------------------------------------------------------------

void game_cmd_args_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    u8* ptr = (u8*)params;
    ptr += (s32)proxy->target;
    *(const char**)res = (const char*)ptr;
}

// ----------------------------------------------------------------

void game_signon_state_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = **(s32**)proxy->target;
}

void game_signon_state_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = *(s32*)proxy->target;
}

// ----------------------------------------------------------------

void game_paint_time_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = **(s32**)proxy->target;
}

void game_paint_time_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = *(s32*)proxy->target;
}

void game_paint_time_proxy_2(GameFnProxy* proxy, void* params, void* res)
{
    *(s64*)res = **(s64**)proxy->target;
}

// ----------------------------------------------------------------

void game_local_player_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(void**)res = **(void***)proxy->target;
}

void game_local_player_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    *(void**)res = *(void**)proxy->target;
}

// ----------------------------------------------------------------

void game_spec_target_or_local_player_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = void*(__cdecl*)();
    DestFn fn = (DestFn)proxy->target;
    *(void**)res = fn();
}

// ----------------------------------------------------------------

void game_paint_buffer_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(GameSndSample0**)res = **(GameSndSample0***)proxy->target;
}

void game_paint_buffer_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    *(GameSndSample0**)res = *(GameSndSample0**)proxy->target;
}

// ----------------------------------------------------------------

void game_cvar_restrict_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    // If the game has a restriction that prevents console variables from changing when in game or demo playback.
    // This changes so it is always allowed to change any console variable.
    s32 cvar_restrict_patch_bytes = 0x00;
    game_apply_patch(proxy->target, &cvar_restrict_patch_bytes, sizeof(cvar_restrict_patch_bytes));
}

// ----------------------------------------------------------------

void game_d3d9ex_device_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(void**)res = **(void***)proxy->target;
}

void game_d3d9ex_device_proxy_1(GameFnProxy* proxy, void* params, void* res)
{
    *(void**)res = *(void**)proxy->target;
}

// ----------------------------------------------------------------

void game_demo_player_playback_tick_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = s32(__fastcall*)(void* p, void* edx);
    DestFn func = (DestFn)proxy->target;
    *(s32*)res = func(proxy->extra[0], NULL);
}

// ----------------------------------------------------------------

void game_engine_client_command(const char* cmd)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_CORE);
    GAME_CALL_PROXY(game_state.search_desc.engine_client_command_proxy, (void*)cmd, NULL);
}

const char* game_get_cmd_args(void* ptr)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_CORE);

    const char* ret = NULL;
    GAME_CALL_PROXY(game_state.search_desc.cmd_args_proxy, ptr, &ret);
    return ret;
}

SvrVec3 game_get_entity_velocity(void* entity)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);

    SvrVec3 ret = {};
    GAME_CALL_PROXY(game_state.search_desc.entity_velocity_proxy, entity, &ret);
    return ret;
}

u32 game_get_player_buttons(void* player)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);
    assert(game_state.search_desc.caps & GAME_CAP_HAS_INPUT);

    u32 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.player_buttons_proxy, player, &ret);
    return ret;
}

void* game_get_player_by_index(s32 idx)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);

    void* ret = NULL;
    GAME_CALL_PROXY(game_state.search_desc.player_by_index_proxy, &idx, &ret);
    return ret;
}

void* game_get_local_player()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);

    void* ret = NULL;
    GAME_CALL_PROXY(game_state.search_desc.local_player_proxy, NULL, &ret);
    return ret;
}

s32 game_get_spec_target()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);

    s32 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.spec_target_proxy, NULL, &ret);
    return ret;
}

void* game_get_spec_target_or_local_player()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_LOCAL_PLAYER);

    void* ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.spec_target_or_local_player_proxy, NULL, &ret);
    return ret;
}

s32 game_get_signon_state()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUTOSTOP);

    s32 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.signon_state_proxy, NULL, &ret);
    return ret;
}

s32 game_get_snd_paint_time_0()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUDIO);

    s32 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.snd_paint_time_proxy, NULL, &ret);
    return ret;
}

s64 game_get_snd_paint_time_1()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUDIO);
    assert(game_state.search_desc.caps & GAME_CAP_64_BIT_AUDIO_TIME);

    s64 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.snd_paint_time_proxy, NULL, &ret);
    return ret;
}

GameSndSample0* game_get_snd_paint_buffer_0()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUDIO);

    GameSndSample0* ret = NULL;
    GAME_CALL_PROXY(game_state.search_desc.snd_paint_buffer_proxy, NULL, &ret);
    return ret;
}

void* game_get_d3d9ex_device()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_VIDEO);
    assert(game_state.search_desc.caps & GAME_CAP_D3D9EX_VIDEO);

    void* ret = NULL;
    GAME_CALL_PROXY(game_state.search_desc.d3d9ex_device_proxy, NULL, &ret);
    return ret;
}

s32 game_get_demo_player_playback_tick()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_STUDIO);

    s32 ret = 0;
    GAME_CALL_PROXY(game_state.search_desc.demo_player_playback_tick_proxy, NULL, &ret);
    return ret;
}

void game_proxies_init()
{
    GAME_CALL_PROXY(game_state.search_desc.cvar_patch_restrict_proxy, NULL, NULL);
}

#undef GAME_CALL_PROXY
