#pragma once
#include "svr_common.h"

const s32 MAX_VELOC_FONT_NAME = 128;

struct MovieProfile
{
    s32 movie_fps;

    // SW encoding:
    const char* sw_encoder;
    s32 sw_crf;
    const char* sw_x264_preset;
    s32 sw_x264_intra;

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
    enum DWRITE_FONT_STRETCH veloc_font_stretch;
    enum DWRITE_TEXT_ALIGNMENT veloc_text_align;
    enum DWRITE_PARAGRAPH_ALIGNMENT veloc_para_align;
    s32 veloc_padding;
};

bool read_profile(const char* full_profile_path, MovieProfile* p);
