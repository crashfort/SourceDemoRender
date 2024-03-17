#pragma once
#include "svr_common.h"
#include "svr_steam_common.h"

// Used by the launcher as parameter for the init exports in svr_standalone.dll.
struct SvrGameInitData
{
    const char* svr_path;
    SteamAppId app_id;
};

using SvrGameInitFuncType = void(__cdecl*)(SvrGameInitData* init_data);
