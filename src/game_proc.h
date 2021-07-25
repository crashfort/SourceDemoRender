#pragma once
#include "svr_common.h"

void proc_init();
void proc_start(const char* dest, const char* profile, void* game_share_h);
void proc_frame();
void proc_end();
s32 proc_get_game_rate();
