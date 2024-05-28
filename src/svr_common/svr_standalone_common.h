#pragma once
#include "svr_common.h"

// Used by the launcher as parameter for the init exports in svr_standalone.dll.
struct SvrGameInitData
{
    const char* svr_path; // Does not end with a slash.
    u32 unused;
};

using SvrGameInitFuncType = void(__cdecl*)(SvrGameInitData* init_data);
