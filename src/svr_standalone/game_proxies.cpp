#include "game_priv.h"

void game_get_spec_target_proxy_0(GameFnProxy* proxy, void* params, void* res)
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

// ----------------------------------------------------------------

void game_get_player_by_index_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    using DestFn = void*(__cdecl*)(s32 index);
    DestFn fn = (DestFn)proxy->target;
    *(void**)res = fn(*(s32*)params);
}

// ----------------------------------------------------------------

void game_get_velocity_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    u8* ptr = (u8*)params;
    ptr += (s32)proxy->target;
    memcpy(res, ptr, sizeof(SvrVec3));
}

// ----------------------------------------------------------------

void game_get_cmd_args_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    u8* ptr = (u8*)params;
    ptr += (s32)proxy->target;
    *(const char**)res = (const char*)ptr;
}

// ----------------------------------------------------------------

void game_get_signon_state_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = **(s32**)proxy->target;
}

// ----------------------------------------------------------------

void game_get_paint_time_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(s32*)res = **(s32**)proxy->target;
}

// ----------------------------------------------------------------

void game_get_local_player_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(void**)res = **(void***)proxy->target;
}

// ----------------------------------------------------------------

void game_get_paint_buffer_proxy_0(GameFnProxy* proxy, void* params, void* res)
{
    *(GameSndSample0**)res = **(GameSndSample0***)proxy->target;
}

// ----------------------------------------------------------------

void game_engine_client_command(const char* cmd)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_CORE);
    game_state.search_desc.engine_client_command_proxy.proxy(&game_state.search_desc.engine_client_command_proxy, (void*)cmd, NULL);
}

const char* game_get_cmd_args(void* ptr)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_CORE);

    const char* ret = NULL;
    game_state.search_desc.get_cmd_args_proxy.proxy(&game_state.search_desc.get_cmd_args_proxy, ptr, &ret);
    return ret;
}

SvrVec3 game_get_entity_velocity(void* entity)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_VELO);

    SvrVec3 ret = {};
    game_state.search_desc.get_entity_velocity_proxy.proxy(&game_state.search_desc.get_entity_velocity_proxy, entity, &ret);
    return ret;
}

void* game_get_player_by_index(s32 idx)
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_VELO);

    void* ret = NULL;
    game_state.search_desc.get_player_by_index_proxy.proxy(&game_state.search_desc.get_player_by_index_proxy, &idx, &ret);
    return ret;
}

void* game_get_local_player()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_VELO);

    void* ret = NULL;
    game_state.search_desc.get_local_player_proxy.proxy(&game_state.search_desc.get_local_player_proxy, NULL, &ret);
    return ret;
}

s32 game_get_spec_target()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_VELO);

    s32 ret = 0;
    game_state.search_desc.get_spec_target_proxy.proxy(&game_state.search_desc.get_spec_target_proxy, NULL, &ret);
    return ret;
}

s32 game_get_signon_state()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUTOSTOP);

    s32 ret = 0;
    game_state.search_desc.get_signon_state_proxy.proxy(&game_state.search_desc.get_signon_state_proxy, NULL, &ret);
    return ret;
}

s32 game_get_snd_paint_time_0()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUDIO);

    s32 ret = 0;
    game_state.search_desc.snd_get_paint_time_proxy.proxy(&game_state.search_desc.snd_get_paint_time_proxy, NULL, &ret);
    return ret;
}

GameSndSample0* game_get_snd_paint_buffer_0()
{
    assert(game_state.search_desc.caps & GAME_CAP_HAS_AUDIO);

    GameSndSample0* ret = NULL;
    game_state.search_desc.snd_get_paint_buffer_proxy.proxy(&game_state.search_desc.snd_get_paint_buffer_proxy, NULL, &ret);
    return ret;
}
