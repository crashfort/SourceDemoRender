#pragma once
#include "svr_common.h"

const s32 MAX_VELOC_FONT_NAME = 128;

struct MovieProfile
{
    // Movie encoding:
    s32 video_fps;
    const char* video_encoder;
    s32 video_x264_crf;
    const char* video_x264_preset;
    s32 video_x264_intra;
    const char* video_dnxhr_profile;
    s32 audio_enabled;
    const char* audio_encoder;

    // Mosample:
    s32 mosample_enabled;
    s32 mosample_mult;
    float mosample_exposure;

    // Veloc:
    s32 veloc_enabled;
    char veloc_font[MAX_VELOC_FONT_NAME];
    s32 veloc_font_size;
    s32 veloc_font_color[4];
    s32 veloc_font_border_color[4];
    s32 veloc_font_border_size;
    enum DWRITE_FONT_STYLE veloc_font_style;
    enum DWRITE_FONT_WEIGHT veloc_font_weight;
    s32 veloc_align[2];
};

bool read_profile(const char* full_profile_path, MovieProfile* p);
