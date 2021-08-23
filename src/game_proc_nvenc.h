#pragma once
#include "svr_common.h"

// If NVENC is available on this system.
bool proc_is_nvenc_supported();

// For when NVENC is supported.
void proc_init_nvenc();

// When movie wants to start.
void proc_start_nvenc(s32 width, s32 height, s32 fps);

void proc_nvenc_frame();

// When movie wants to stop.
void proc_stop_nvenc();
