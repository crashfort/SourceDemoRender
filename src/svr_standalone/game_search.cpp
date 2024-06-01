#include "game_priv.h"

// Type agnostic address search and proxy redirections.

// For x86 CS:S.
GameFnProxy game_get_snd_paint_time_proxy_0()
{
    // Search for "S_Update_Guts", find subtraction with global.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "2B 05 ?? ?? ?? ?? 0F 48 C1 89 45 FC 85 C0", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_time_proxy_0;
    return px;
}

// For x86 BM:S.
GameFnProxy game_get_snd_paint_time_proxy_1()
{
    u8* addr = (u8*)game_scan_pattern("engine.dll", "2B 35 ?? ?? ?? ?? 0F 48 F0", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_time_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_snd_paint_time_proxy_2()
{
    // Search for "Start profiling MIX_PaintChannels\n", find assignment from global at start, and assignment to global at end.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "8B 3D ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    u32 offset = *(u32*)addr;
    addr += offset;
    addr += 4;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_time_proxy_1;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_snd_paint_time_proxy_3()
{
    // Search for "Start profiling MIX_PaintChannels\n", find assignment from global at start, and assignment to global at end.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "66 0F 13 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 51 68", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 4;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_time_proxy_2;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnOverride game_get_snd_tx_stereo_override_0()
{
    // Search for "DS_STEREO", find call to path with 2 channels and 16 bits, use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 51 53 56 57 E8 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D", NULL);
    ov.override = game_snd_tx_stereo_override_0;
    return ov;
}

// ----------------------------------------------------------------

// For x64 TF2.
GameFnOverride game_get_snd_device_tx_samples_override_0()
{
    // Search for "Game Volume: %1.2f", find usage of volume cvar, x-ref to find S_GetMasterVolume, x-ref to find IAudioDevice2::TransferSamples.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "48 89 5C 24 ?? 48 89 4C 24 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? B8 ?? ?? ?? ?? E8 ?? ?? ?? ??", NULL);
    ov.override = game_snd_device_tx_samples_override_0;
    return ov;
}

// For x86 CS:GO.
GameFnOverride game_get_snd_device_tx_samples_override_1()
{
    // Search for "Game Volume: %1.2f", find usage of volume cvar, x-ref to find S_GetMasterVolume, x-ref to find IAudioDevice2::TransferSamples.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1", NULL);
    ov.override = game_snd_device_tx_samples_override_0;
    return ov;
}

// ----------------------------------------------------------------

// For x64 TF2.
GameFnProxy game_get_snd_get_paint_buffer_proxy_0()
{
    // Find IAudioDevice2::TransferSamples, find assignment from global.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "48 8B 3D ?? ?? ?? ?? 48 89 B5 ?? ?? ?? ?? 48 89 9D ?? ?? ?? ?? 0F 29 B4 24 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 3;

    u32 offset = *(u32*)addr;
    addr += offset;
    addr += 4;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_buffer_proxy_1;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_snd_get_paint_buffer_proxy_1()
{
    // Find IAudioDevice2::TransferSamples, find assignment from global.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "8B 35 ?? ?? ?? ?? 89 45 F8 A1 ?? ?? ?? ?? 57 8B 3D ?? ?? ?? ?? 89 45 FC", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_paint_buffer_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnOverride game_get_snd_paint_chans_override_0()
{
    // Search for "MIX_PaintChannels", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 33 DB 89 5D D0 89 5D D4", NULL);
    ov.override = game_snd_paint_chans_override_0;
    return ov;
}

// For x86 HDTF.
GameFnOverride game_get_snd_paint_chans_override_1()
{
    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 33 DB 89 5D C8 89 5D CC", NULL);
    ov.override = game_snd_paint_chans_override_0;
    return ov;
}

// For x86 BM:S.
GameFnOverride game_get_snd_paint_chans_override_2()
{
    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 81 EC C4 01 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 ?? 8B 0D", NULL);
    ov.override = game_snd_paint_chans_override_0;
    return ov;
}

// For x64 TF2.
GameFnOverride game_get_snd_paint_chans_override_3()
{
    // Search for "MIX_PaintChannels", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "48 8B C4 88 50 10 89 48 08 53 48 81 EC ?? ?? ?? ?? 48 89 78 E0 33 FF 4C 89 68 D0 4C 89 78 C0", NULL);
    ov.override = game_snd_paint_chans_override_0;
    return ov;
}

// For x86 CS:GO.
GameFnOverride game_get_snd_paint_chans_override_4()
{
    // Search for "MIX_PaintChannels", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? A0 ?? ?? ?? ?? 53 56 88 45 ?? A1", NULL);
    ov.override = game_snd_paint_chans_override_1;
    return ov;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_player_by_index_proxy_0()
{
    // Find UTIL_PlayerByIndex, use function (see Source 2013 SDK).

    GameFnProxy px;
    px.target = game_scan_pattern("client.dll", "55 8B EC 8B 0D ?? ?? ?? ?? 56 FF 75 08 E8 ?? ?? ?? ?? 8B F0 85 F6 74 15 8B 16 8B CE 8B 92 ?? ?? ?? ?? FF D2 84 C0 74 05 8B C6 5E 5D C3 33 C0 5E 5D C3", NULL);
    px.proxy = game_player_by_index_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_player_by_index_proxy_1()
{
    // Search for "achievement_earned", find usage with "player" and "achievement".

    GameFnProxy px;
    px.target = game_scan_pattern("client.dll", "40 53 48 83 EC 20 8B D1 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 74 19 48 8B 00 48 8B CB", NULL);
    px.proxy = game_player_by_index_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_spec_target_proxy_0()
{
    // Find GetSpectatorTarget, use function.

    GameFnProxy px;
    px.target = game_scan_pattern("client.dll", "E8 ?? ?? ?? ?? 85 C0 74 16 8B 10 8B C8 FF 92 ?? ?? ?? ?? 85 C0 74 08 8D 48 08 8B 01 FF 60 24 33 C0 C3", NULL);
    px.proxy = game_spec_target_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_spec_target_proxy_1()
{
    // Search for "spec_target_updated", find function whose result is range checked between 0 and 64.

    GameFnProxy px;
    px.target = game_scan_pattern("client.dll", "48 83 EC 28 E8 ?? ?? ?? ?? 48 85 C0 74 21 48 8B 10 48 8B C8 FF 92 ?? ?? ?? ?? 48 85 C0 74 10 48 8D 48 10", NULL);
    px.proxy = game_spec_target_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnOverride game_get_end_movie_override_0()
{
    // Search for "Stopped recording movie...\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "80 3D ?? ?? ?? ?? ?? 75 0F 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 04 C3 E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 59 C3", NULL);
    ov.override = game_end_movie_override_0;
    return ov;
}

// For x64 TF2.
GameFnOverride game_get_end_movie_override_1()
{
    // Search for "Stopped recording movie...\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "48 83 EC 28 80 3D ?? ?? ?? ?? ?? 75 12 48 8D 0D ?? ?? ?? ?? 48 83 C4 28 48 FF 25 ?? ?? ?? ?? E8 ?? ?? ?? ??", NULL);
    ov.override = game_end_movie_override_0;
    return ov;
}

// For x86 CS:GO.
GameFnOverride game_get_end_movie_override_2()
{
    // Search for "Stopped recording movie...\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "80 3D ?? ?? ?? ?? ?? 75 0F 68", NULL);
    ov.override = game_end_movie_override_0;
    return ov;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnOverride game_get_start_movie_override_0()
{
    // Search for "Already recording movie!\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 83 EC 08 83 3D ?? ?? ?? ?? ?? 0F 85", NULL);
    ov.override = game_start_movie_override_0;
    return ov;
}

// For x64 TF2.
GameFnOverride game_get_start_movie_override_1()
{
    // Search for "Already recording movie!\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "41 56 48 83 EC 70 83 3D ?? ?? ?? ?? ?? 4C 8B F1 0F 85 ?? ?? ?? ?? 8B 11 4C 89 64 24 ??", NULL);
    ov.override = game_start_movie_override_0;
    return ov;
}

// For x86 CS:GO.
GameFnOverride game_get_start_movie_override_2()
{
    // Search for "Already recording movie!\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 83 EC 08 53 56 57 8B 7D 08 8B 1F 83 FB 02 7D 5F", NULL);
    ov.override = game_start_movie_override_0;
    return ov;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnOverride game_get_eng_filter_time_override_0()
{
    // Search for "sv_cheats is 0 and fps_max is being limited to a minimum of 30 (or set to 0).\n", use function.

    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 51 80 3D ?? ?? ?? ?? ?? 56 8B F1 74", NULL);
    ov.override = game_eng_filter_time_override_0;
    return ov;
}

// For x86 BM:S.
GameFnOverride game_get_eng_filter_time_override_1()
{
    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 83 EC 10 80 3D ?? ?? ?? ?? ?? 56", NULL);
    ov.override = game_eng_filter_time_override_0;
    return ov;
}

// For x64 TF2.
GameFnOverride game_get_eng_filter_time_override_2()
{
    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "40 53 48 83 EC 40 80 3D ?? ?? ?? ?? ?? 48 8B D9 0F 29 74 24 ?? 0F 28 F1 74 2B 80 3D ?? ?? ?? ?? ?? 75 22", NULL);
    ov.override = game_eng_filter_time_override_0;
    return ov;
}

#ifndef _WIN64
// For x86 CS:GO.
GameFnOverride game_get_eng_filter_time_override_3()
{
    GameFnOverride ov;
    ov.target = game_scan_pattern("engine.dll", "55 8B EC 83 EC 0C 80 3D ?? ?? ?? ?? ?? 56", NULL);
    ov.override = game_eng_filter_time_override_1;
    return ov;
}
#endif

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_signon_state_proxy_0()
{
    // Search for "Playing demo from %s.\n", find global assignment to 2.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 87 ?? ?? ?? ?? 89 87 ?? ?? ?? ?? 8B 45 08", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_signon_state_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_signon_state_proxy_1()
{
    // Search for "Playing demo from %s.\n", find global assignment to 2.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 33 D2 89 87 ?? ?? ?? ?? 33 C9 8B 05 ?? ?? ?? ?? 89 87 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;

    u32 offset = *(u32*)addr;
    addr += offset;
    addr += 8;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_signon_state_proxy_1;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_signon_state_proxy_2()
{
    // Search for "Playing demo from %s.\n", find global assignment to 2.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "A1 ?? ?? ?? ?? 33 D2 6A 00 6A 00 33 C9 C7 80", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    void* client_state = **(void***)addr;
    addr = (u8*)client_state;
    addr += 264;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_signon_state_proxy_1;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_local_player_proxy_0()
{
    // Find C_BasePlayer::PostDataUpdate (or search "snd_soundmixer"), find global assignment to s_pLocalPlayer.

    u8* addr = (u8*)game_scan_pattern("client.dll", "A3 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B C8 E8", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_local_player_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_local_player_proxy_1()
{
    // Find C_BasePlayer::PostDataUpdate (or search "snd_soundmixer"), find global assignment to s_pLocalPlayer.

    u8* addr = (u8*)game_scan_pattern("client.dll", "48 89 05 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 8B 01 FF 50 68 48 8B C8", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 3;
    u32 offset = *(u32*)addr;
    addr += offset;
    addr += 4;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_local_player_proxy_1;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:GO.
GameFnProxy game_get_spec_target_or_local_player_proxy_0()
{
    u8* addr = (u8*)game_scan_pattern("client.dll", "55 8B EC 8B 4D 04 56 57 E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 85 F6 74 57 8B 06 8B CE", NULL);

    if (addr == NULL)
    {
        return {};
    }

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_spec_target_or_local_player_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_d3d9ex_device_proxy_0()
{
    // Search for "D3DQUERYTYPE_EVENT not available on this driver\n", find global near comparison of 0x8876086A.

    u8* addr = (u8*)game_scan_pattern("shaderapidx9.dll", "A1 ?? ?? ?? ?? 6A 00 56 6A 00 8B 08 6A 15 68 ?? ?? ?? ?? 6A 00 6A 01 6A 01 50 FF 51 5C 85 C0 79 06 C7 06", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_d3d9ex_device_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_d3d9ex_device_proxy_1()
{
    // Search for "D3DQUERYTYPE_EVENT not available on this driver\n", find global near comparison of 0x8876086A.

    u8* addr = (u8*)game_scan_pattern("shaderapidx9.dll", "48 8B 0D ?? ?? ?? ?? 8D 46 01 48 63 D0 4C 8D 85 ?? ?? ?? ?? 4C 8B 09 4D 8D 04 D0", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 3;
    u32 offset = *(u32*)addr;
    addr += offset;
    addr += 4;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_d3d9ex_device_proxy_1;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_d3d9ex_device_proxy_2()
{
    // Search for "D3DQUERYTYPE_EVENT not available on this driver\n", find global near comparison of 0x8876086A.

    u8* addr = (u8*)game_scan_pattern("shaderapidx9.dll", "A1 ?? ?? ?? ?? 6A 00 56 6A 00 8B 08 6A 15 68 ?? ?? ?? ?? 6A 00 6A 01 6A 01 50 FF 51 5C 85 C0 79 06 C7 06", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_d3d9ex_device_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_cvar_restrict_proxy_0()
{
    // Search for "Can't set %s in multiplayer\n", find AND with 0x400000 (FCVAR_NOT_CONNECTED).

    u8* addr = (u8*)game_scan_pattern("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 58 83 3D", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_cvar_restrict_proxy_0;
    return px;
}

// For x86 BM:S.
GameFnProxy game_get_cvar_restrict_proxy_1()
{
    u8* addr = (u8*)game_scan_pattern("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 52 83 3D", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_cvar_restrict_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_cvar_restrict_proxy_2()
{
    u8* addr = (u8*)game_scan_pattern("engine.dll", "BA ?? ?? ?? ?? 48 8B CB FF 50 10 84 C0 74 58 83 3D ?? ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_cvar_restrict_proxy_0;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_cvar_restrict_proxy_3()
{
    u8* addr = (u8*)game_scan_pattern("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 5D A1 ?? ?? ?? ?? 83 B8", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 1;

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_cvar_restrict_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_engine_client_command_proxy_0()
{
    // Search for "Cbuf_Execute" and "Cbuf_AddText: buffer overflow\n", find IVEngineClient::ExecuteClientCmd which calls both Cbuf_AddText and Cbuf_Execute.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "55 8B EC FF 75 08 E8 ?? ?? ?? ?? 83 C4 04 E8 ?? ?? ?? ?? 5D C2 04 00", NULL);

    if (addr == NULL)
    {
        return {};
    }

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_engine_client_command_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_engine_client_command_proxy_1()
{
    // Search for "Cbuf_Execute" and "Cbuf_AddText: buffer overflow\n", find IVEngineClient::ExecuteClientCmd which calls both Cbuf_AddText and Cbuf_Execute.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "48 83 EC 28 48 8B CA E8 ?? ?? ?? ?? 48 83 C4 28 E9 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_engine_client_command_proxy_1;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_engine_client_command_proxy_2()
{
    // Search for "Executing command outside main loop thread\n" find IVEngineClient::ExecuteClientCmd which calls both Cbuf_AddText and Cbuf_Execute.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "55 8B EC 8B 55 08 33 C9 6A 00 6A 00 E8 ?? ?? ?? ?? 83 C4 08 E8 ?? ?? ?? ?? 5D C2 04 00", NULL);

    if (addr == NULL)
    {
        return {};
    }

    GameFnProxy px;
    px.target = addr;
    px.proxy = game_engine_client_command_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_velocity_proxy_0()
{
    // Search for "m_vecVelocity[0]", find RecvProxy_LocalVelocityX and offset to C_BaseEntity::m_vecVelocity.

    u8* addr = (u8*)game_scan_pattern("client.dll", "8B 81 ?? ?? ?? ?? 89 45 F4 F3 0F 10 45 ?? 8B 81 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;
    s32 offset = *(s32*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_velocity_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_velocity_proxy_1()
{
    // Search for "m_vecVelocity[0]", find offset to C_BaseEntity::m_vecVelocity.

    u8* addr = (u8*)game_scan_pattern("client.dll", "41 B8 ?? ?? ?? ?? 48 89 44 24 ?? 44 8D 4D 04 48 8D 15 ?? ?? ?? ?? 89 6C 24 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? 48 89 44 24 ?? 44 8D 4D 04 48 8D 15 ?? ?? ?? ?? 89 6C 24 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;
    s32 offset = *(s32*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_velocity_proxy_0;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_velocity_proxy_2()
{
    // Search for "m_vecVelocity[0]", find RecvProxy_LocalVelocityX and offset to C_BaseEntity::m_vecVelocity.

    u8* addr = (u8*)game_scan_pattern("client.dll", "0F 2E 8E ?? ?? ?? ?? 9F F6 C4 44 7A 24 F3 0F 10 45 ?? 0F 2E 86 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 3;
    s32 offset = *(s32*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_velocity_proxy_0;
    return px;
}

// ----------------------------------------------------------------

// For x86 CS:S.
GameFnProxy game_get_cmd_args_proxy_0()
{
    // Search for "hltv_message", search for "text" and find full args offset into CCommand.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "83 C1 ?? 03 CA EB 05 B9 ?? ?? ?? ?? 8B 06", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 2;
    s32 offset = *(u8*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_cmd_args_proxy_0;
    return px;
}

// For x64 TF2.
GameFnProxy game_get_cmd_args_proxy_1()
{
    // Search for "hltv_message", search for "text" and find full args offset into CCommand.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "4C 8D 43 ?? 4C 03 C0 EB 07 4C 8D 05 ?? ?? ?? ??", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 3;
    s32 offset = *(u8*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_cmd_args_proxy_0;
    return px;
}

// For x86 CS:GO.
GameFnProxy game_get_cmd_args_proxy_2()
{
    // Search for "hltv_message", search for "text" and find full args offset into CCommand.

    u8* addr = (u8*)game_scan_pattern("engine.dll", "74 06 83 C0 ?? 03 C1 C3", NULL);

    if (addr == NULL)
    {
        return {};
    }

    addr += 4;
    s32 offset = *(u8*)addr;

    GameFnProxy px;
    px.target = (void*)offset;
    px.proxy = game_cmd_args_proxy_0;
    return px;
}

// ----------------------------------------------------------------

struct GameOverrideOpt
{
    bool cond; // Condition such as architecture.
    GameFnOverride(*func)();
    const char* libs[4]; // Libraries referenced.
    GameCaps append_caps;
};

struct GameProxyOpt
{
    bool cond; // Condition such as architecture.
    GameFnProxy(*func)();
    const char* libs[4]; // Libraries referenced.
    GameCaps append_caps;
};

struct GameCondCaps
{
    bool cond;
    GameCaps caps;
};

GameProxyOpt GAME_SND_PAINT_TIME_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_snd_paint_time_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_snd_paint_time_proxy_1, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_snd_paint_time_proxy_2, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_snd_paint_time_proxy_3, { "engine.dll" }, GAME_CAP_64_BIT_AUDIO_TIME },
};

GameOverrideOpt GAME_SND_TX_STEREO_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_tx_stereo_override_0, { "engine.dll" }, 0 },
};

GameOverrideOpt GAME_SND_DEVICE_TX_SAMPLES_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X64(), game_get_snd_device_tx_samples_override_0, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_device_tx_samples_override_1, { "engine.dll" }, 0 },
};

GameProxyOpt GAME_SND_PAINT_BUFFER_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X64(), game_get_snd_get_paint_buffer_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_snd_get_paint_buffer_proxy_1, { "engine.dll" }, 0 },
};

GameOverrideOpt GAME_SND_PAINT_CHANS_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_paint_chans_override_0, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_paint_chans_override_1, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_paint_chans_override_2, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X64(), game_get_snd_paint_chans_override_3, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_snd_paint_chans_override_4, { "engine.dll" }, 0 },
};

GameProxyOpt GAME_PLAYER_BY_INDEX_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_player_by_index_proxy_0, { "client.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_player_by_index_proxy_1, { "client.dll" }, 0 },
};

GameProxyOpt GAME_SPEC_TARGET_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_spec_target_proxy_0, { "client.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_spec_target_proxy_1, { "client.dll" }, 0 },
};

GameOverrideOpt GAME_END_MOVIE_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X86(), game_get_end_movie_override_0, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X64(), game_get_end_movie_override_1, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_end_movie_override_2, { "engine.dll" }, 0 },
};

GameOverrideOpt GAME_START_MOVIE_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X86(), game_get_start_movie_override_0, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X64(), game_get_start_movie_override_1, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_start_movie_override_2, { "engine.dll" }, 0 },
};

GameOverrideOpt GAME_FILTER_TIME_OVERRIDES[] =
{
    GameOverrideOpt { SVR_IS_X86(), game_get_eng_filter_time_override_0, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X86(), game_get_eng_filter_time_override_1, { "engine.dll" }, 0 },
    GameOverrideOpt { SVR_IS_X64(), game_get_eng_filter_time_override_2, { "engine.dll" }, 0 },

#ifndef _WIN64
    GameOverrideOpt { SVR_IS_X86(), game_get_eng_filter_time_override_3, { "engine.dll" }, 0 },
#endif
};

GameProxyOpt GAME_SIGNON_STATE_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_signon_state_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_signon_state_proxy_1, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_signon_state_proxy_2, { "engine.dll" }, 0 },
};

GameProxyOpt GAME_LOCAL_PLAYER_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_local_player_proxy_0, { "client.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_local_player_proxy_1, { "client.dll" }, 0 },
};

GameProxyOpt GAME_SPEC_TARGET_OR_LOCAL_PLAYER_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_spec_target_or_local_player_proxy_0, { "client.dll" }, 0 },
};

GameProxyOpt GAME_D3D9EX_DEVICE_PTR_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_d3d9ex_device_proxy_0, { "shaderapidx9.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_d3d9ex_device_proxy_1, { "shaderapidx9.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_d3d9ex_device_proxy_2, { "shaderapidx9.dll" }, 0 },
};

GameProxyOpt GAME_CVAR_RESTRICT_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_cvar_restrict_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_cvar_restrict_proxy_1, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_cvar_restrict_proxy_2, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_cvar_restrict_proxy_3, { "engine.dll" }, 0 },
};

GameProxyOpt GAME_ENGINE_CLIENT_COMMAND_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_engine_client_command_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_engine_client_command_proxy_1, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_engine_client_command_proxy_2, { "engine.dll" }, 0 },
};

GameProxyOpt GAME_ENTITY_VELOCITY_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_velocity_proxy_0, { "client.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_velocity_proxy_1, { "client.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_velocity_proxy_2, { "client.dll" }, 0 },
};

GameProxyOpt GAME_CMD_ARGS_PROXIES[] =
{
    GameProxyOpt { SVR_IS_X86(), game_get_cmd_args_proxy_0, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X64(), game_get_cmd_args_proxy_1, { "engine.dll" }, 0 },
    GameProxyOpt { SVR_IS_X86(), game_get_cmd_args_proxy_2, { "engine.dll" }, 0 },
};

GameFnOverride game_select_opts(GameOverrideOpt* opts, s32 num, const char* array_name, GameCaps* append_caps)
{
    for (s32 i = 0; i < num; i++)
    {
        GameOverrideOpt opt = opts[i];

        if (opt.cond)
        {
            GameFnOverride ov = opt.func();

            if (game_is_valid(ov))
            {
                if (append_caps)
                {
                    *append_caps |= opt.append_caps;
                }

                svr_log("Using pattern %d in array %s\n", i, array_name);
                return ov;
            }
        }
    }

    svr_log("No pattern used in array %s\n", array_name);
    return {};
}

GameFnProxy game_select_opts(GameProxyOpt* opts, s32 num, const char* array_name, GameCaps* append_caps)
{
    for (s32 i = 0; i < num; i++)
    {
        GameProxyOpt opt = opts[i];

        if (opt.cond)
        {
            GameFnProxy px = opt.func();

            if (game_is_valid(px))
            {
                if (append_caps)
                {
                    *append_caps |= opt.append_caps;
                }

                svr_log("Using pattern %d in array %s\n", i, array_name);
                return px;
            }
        }
    }

    svr_log("No pattern used in array %s\n", array_name);
    return {};
}

bool game_lib_already_added(SvrDynArray<const char*>* libs, const char* test)
{
    for (s32 i = 0; i < libs->size; i++)
    {
        if (!strcmp(libs->at(i), test))
        {
            return true;
        }
    }

    return false;
}

void game_add_libs(const char** libs, s32 num, SvrDynArray<const char*>* dest)
{
    for (s32 i = 0; i < num && libs[i]; i++)
    {
        if (!game_lib_already_added(dest, libs[i]))
        {
            dest->push(libs[i]);
        }
    }
}

void game_select_libs(GameOverrideOpt* opts, s32 num, SvrDynArray<const char*>* dest)
{
    for (s32 i = 0; i < num; i++)
    {
        GameOverrideOpt opt = opts[i];

        if (opt.cond)
        {
            game_add_libs(opt.libs, SVR_ARRAY_SIZE(opt.libs), dest);
        }
    }
}

void game_select_libs(GameProxyOpt* opts, s32 num, SvrDynArray<const char*>* dest)
{
    for (s32 i = 0; i < num; i++)
    {
        GameProxyOpt opt = opts[i];

        if (opt.cond)
        {
            game_add_libs(opt.libs, SVR_ARRAY_SIZE(opt.libs), dest);
        }
    }
}

const s32 GAME_SEARCH_LIB_WAIT_TIME = 120;

// Wait for all the libraries specified in the patterns.
void game_search_wait_for_libs()
{
    SvrDynArray<const char*> list = {};
    list.push("tier0.dll"); // Hack for now in order to be sure the console is loaded for game_console.cpp.

#define SELECT_LIBS(OPTS) game_select_libs(OPTS, SVR_ARRAY_SIZE(OPTS), &list)

    // Core required:
    SELECT_LIBS(GAME_START_MOVIE_OVERRIDES);
    SELECT_LIBS(GAME_END_MOVIE_OVERRIDES);
    SELECT_LIBS(GAME_FILTER_TIME_OVERRIDES);
    SELECT_LIBS(GAME_D3D9EX_DEVICE_PTR_PROXIES);
    SELECT_LIBS(GAME_CVAR_RESTRICT_PROXIES);
    SELECT_LIBS(GAME_ENGINE_CLIENT_COMMAND_PROXIES);
    SELECT_LIBS(GAME_CMD_ARGS_PROXIES);

    // Velo required:
    SELECT_LIBS(GAME_ENTITY_VELOCITY_PROXIES);

    // Velo dumb required:
    SELECT_LIBS(GAME_PLAYER_BY_INDEX_PROXIES);
    SELECT_LIBS(GAME_SPEC_TARGET_PROXIES);
    SELECT_LIBS(GAME_LOCAL_PLAYER_PROXIES);

    // Velo smart required:
    SELECT_LIBS(GAME_SPEC_TARGET_OR_LOCAL_PLAYER_PROXIES);

    // Audio required:
    SELECT_LIBS(GAME_SND_PAINT_TIME_PROXIES);
    SELECT_LIBS(GAME_SND_PAINT_CHANS_OVERRIDES);

    // Audio variant 1 required:
    SELECT_LIBS(GAME_SND_TX_STEREO_OVERRIDES);

    // Audio variant 2 required:
    SELECT_LIBS(GAME_SND_DEVICE_TX_SAMPLES_OVERRIDES);
    SELECT_LIBS(GAME_SND_PAINT_BUFFER_PROXIES);

    // Autostop required:
    SELECT_LIBS(GAME_SIGNON_STATE_PROXIES);

#undef SELECT_LIBS

    svr_log("Waiting for game libraries:\n");

    for (s32 i = 0; i < list.size; i++)
    {
        svr_log("- %s\n", list[i]);
    }

    if (!game_wait_for_libs_to_load(list.mem, list.size, GAME_SEARCH_LIB_WAIT_TIME))
    {
        svr_log("Not all libraries loaded in time\n");
        game_init_error("Mismatch between game version and supported SVR version. Ensure you are using the latest version of SVR and upload your SVR_LOG.txt.");
    }

    list.free();
}

GameCaps game_select_caps(GameCondCaps* caps, s32 num)
{
    GameCaps ret = 0;

    for (s32 i = 0; i < num; i++)
    {
        GameCondCaps* c = &caps[i];

        if (c->cond)
        {
            ret |= c->caps;
        }
    }

    return ret;
}

void game_search_fill_desc(GameSearchDesc* desc)
{
    // Try and find a working combination of all possible patterns.

    GameCaps opt_caps = 0;

#define SELECT_OPT(OPTS) game_select_opts(OPTS, SVR_ARRAY_SIZE(OPTS), #OPTS, &opt_caps)
#define ALL_TRUE(OPTS) svr_check_all_true(OPTS, SVR_ARRAY_SIZE(OPTS))
#define ANY_TRUE(OPTS) svr_check_one_true(OPTS, SVR_ARRAY_SIZE(OPTS))
#define SELECT_CAPS(OPTS) game_select_caps(OPTS, SVR_ARRAY_SIZE(OPTS))

    // Core required:
    desc->start_movie_override = SELECT_OPT(GAME_START_MOVIE_OVERRIDES);
    desc->end_movie_override = SELECT_OPT(GAME_END_MOVIE_OVERRIDES);
    desc->filter_time_override = SELECT_OPT(GAME_FILTER_TIME_OVERRIDES);
    desc->cvar_patch_restrict_proxy = SELECT_OPT(GAME_CVAR_RESTRICT_PROXIES);
    desc->engine_client_command_proxy = SELECT_OPT(GAME_ENGINE_CLIENT_COMMAND_PROXIES);
    desc->cmd_args_proxy = SELECT_OPT(GAME_CMD_ARGS_PROXIES);

    // Video variant 1 required:
    desc->d3d9ex_device_proxy = SELECT_OPT(GAME_D3D9EX_DEVICE_PTR_PROXIES);

    // Velo required:
    desc->entity_velocity_proxy = SELECT_OPT(GAME_ENTITY_VELOCITY_PROXIES);

    // Velo dumb required:
    desc->player_by_index_proxy = SELECT_OPT(GAME_PLAYER_BY_INDEX_PROXIES);
    desc->spec_target_proxy = SELECT_OPT(GAME_SPEC_TARGET_PROXIES);
    desc->local_player_proxy = SELECT_OPT(GAME_LOCAL_PLAYER_PROXIES);

    // Velo smart required:
    desc->spec_target_or_local_player_proxy = SELECT_OPT(GAME_SPEC_TARGET_OR_LOCAL_PLAYER_PROXIES);

    // Audio required:
    desc->snd_paint_time_proxy = SELECT_OPT(GAME_SND_PAINT_TIME_PROXIES);
    desc->snd_paint_chans_override = SELECT_OPT(GAME_SND_PAINT_CHANS_OVERRIDES);

    // Engine constants. Maybe should scan for these instead.
    desc->snd_sample_rate = 44100;
    desc->snd_num_channels = 2;
    desc->snd_bit_depth = 16;

    // Audio variant 1 required:
    desc->snd_tx_stereo_override = SELECT_OPT(GAME_SND_TX_STEREO_OVERRIDES);

    // Audio variant 2 required:
    desc->snd_device_tx_samples_override = SELECT_OPT(GAME_SND_DEVICE_TX_SAMPLES_OVERRIDES);
    desc->snd_paint_buffer_proxy = SELECT_OPT(GAME_SND_PAINT_BUFFER_PROXIES);

    // Autostop required:
    desc->signon_state_proxy = SELECT_OPT(GAME_SIGNON_STATE_PROXIES);

    // Engine constants. Maybe should scan for these instead.
    desc->signon_state_none = 0; // Not connected.
    desc->signon_state_full = 6; // Fully connected.

    // See what we actually got.

    bool core_required[] =
    {
        game_is_valid(desc->start_movie_override),
        game_is_valid(desc->end_movie_override),
        game_is_valid(desc->filter_time_override),
        game_is_valid(desc->cvar_patch_restrict_proxy),
        game_is_valid(desc->engine_client_command_proxy),
        game_is_valid(desc->cmd_args_proxy),
    };

    bool velo_base_required[] =
    {
        game_is_valid(desc->entity_velocity_proxy),
    };

    bool velo_v1_required[] =
    {
        ALL_TRUE(velo_base_required),
        game_is_valid(desc->player_by_index_proxy),
        game_is_valid(desc->spec_target_proxy),
        game_is_valid(desc->local_player_proxy),
    };

    bool velo_v2_required[] =
    {
        ALL_TRUE(velo_base_required),
        game_is_valid(desc->spec_target_or_local_player_proxy),
    };

    bool audio_base_required[] =
    {
        game_is_valid(desc->snd_paint_chans_override),
        game_is_valid(desc->snd_paint_time_proxy),
        desc->snd_sample_rate != 0,
        desc->snd_num_channels != 0,
        desc->snd_bit_depth != 0,
    };

    bool audio_v1_required[] =
    {
        ALL_TRUE(audio_base_required),
        game_is_valid(desc->snd_tx_stereo_override),
    };

    bool audio_v1_5_required[] =
    {
        ALL_TRUE(audio_base_required),
        game_is_valid(desc->snd_device_tx_samples_override),
        game_is_valid(desc->snd_paint_buffer_proxy),
        !(opt_caps & GAME_CAP_64_BIT_AUDIO_TIME),
    };

    bool audio_v2_required[] =
    {
        ALL_TRUE(audio_base_required),
        game_is_valid(desc->snd_device_tx_samples_override),
        game_is_valid(desc->snd_paint_buffer_proxy),
        opt_caps & GAME_CAP_64_BIT_AUDIO_TIME,
    };

    bool d3d9ex_required[] =
    {
        game_is_valid(desc->d3d9ex_device_proxy),
    };

    bool autostop_required[] =
    {
        game_is_valid(desc->signon_state_proxy),
        desc->signon_state_none != desc->signon_state_full,
    };

    // Determine caps.

    desc->caps |= opt_caps;

    GameCondCaps caps[] =
    {
        GameCondCaps { ALL_TRUE(core_required), GAME_CAP_HAS_CORE },
        GameCondCaps { ALL_TRUE(audio_v1_required), GAME_CAP_AUDIO_DEVICE_1 },
        GameCondCaps { ALL_TRUE(audio_v1_5_required), GAME_CAP_AUDIO_DEVICE_1_5 },
        GameCondCaps { ALL_TRUE(audio_v2_required), GAME_CAP_AUDIO_DEVICE_2 },
        GameCondCaps { ALL_TRUE(autostop_required), GAME_CAP_HAS_AUTOSTOP },
        GameCondCaps { ALL_TRUE(d3d9ex_required), GAME_CAP_D3D9EX_VIDEO },
        GameCondCaps { ALL_TRUE(velo_v1_required), GAME_CAP_VELO_1 },
        GameCondCaps { ALL_TRUE(velo_v2_required), GAME_CAP_VELO_2 },
    };

    desc->caps |= SELECT_CAPS(caps);

    GameCondCaps extra_caps[] =
    {
        GameCondCaps { (desc->caps & GAME_CAP_D3D9EX_VIDEO) != 0, GAME_CAP_HAS_VIDEO },
        GameCondCaps { (desc->caps & GAME_CAP_AUDIO_DEVICE_1) != 0, GAME_CAP_HAS_AUDIO },
        GameCondCaps { (desc->caps & GAME_CAP_AUDIO_DEVICE_1_5) != 0, GAME_CAP_HAS_AUDIO },
        GameCondCaps { (desc->caps & GAME_CAP_AUDIO_DEVICE_2) != 0, GAME_CAP_HAS_AUDIO },
        GameCondCaps { (desc->caps & GAME_CAP_VELO_1) != 0, GAME_CAP_HAS_VELO },
        GameCondCaps { (desc->caps & GAME_CAP_VELO_2) != 0, GAME_CAP_HAS_VELO },
    };

    desc->caps |= SELECT_CAPS(extra_caps);

#undef SELECT_OPT
#undef ALL_TRUE
#undef ANY_TRUE
#undef SELECT_CAPS
}
