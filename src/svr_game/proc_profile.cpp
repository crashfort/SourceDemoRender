#include "proc_priv.h"

// Profile loading.

// Names for ini.
OptStrIntMapping VELO_FONT_WEIGHT_TABLE[] =
{
    OptStrIntMapping { "thin", DWRITE_FONT_WEIGHT_THIN },
    OptStrIntMapping { "extralight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT },
    OptStrIntMapping { "light", DWRITE_FONT_WEIGHT_LIGHT },
    OptStrIntMapping { "semilight", DWRITE_FONT_WEIGHT_SEMI_LIGHT },
    OptStrIntMapping { "normal", DWRITE_FONT_WEIGHT_NORMAL },
    OptStrIntMapping { "medium", DWRITE_FONT_WEIGHT_MEDIUM },
    OptStrIntMapping { "semibold", DWRITE_FONT_WEIGHT_SEMI_BOLD },
    OptStrIntMapping { "bold", DWRITE_FONT_WEIGHT_BOLD },
    OptStrIntMapping { "extrabold", DWRITE_FONT_WEIGHT_EXTRA_BOLD },
    OptStrIntMapping { "black", DWRITE_FONT_WEIGHT_BLACK },
    OptStrIntMapping { "extrablack", DWRITE_FONT_WEIGHT_EXTRA_BLACK },
};

// Names for ini.
OptStrIntMapping VELO_FONT_STYLE_TABLE[] =
{
    OptStrIntMapping { "normal", DWRITE_FONT_STYLE_NORMAL },
    OptStrIntMapping { "italic", DWRITE_FONT_STYLE_ITALIC },
    OptStrIntMapping { "extraitalic", DWRITE_FONT_STYLE_OBLIQUE },
};

// Names for ini.
OptStrIntMapping VELO_ANCHOR_TABLE[] =
{
    OptStrIntMapping { "left", VELO_ANCHOR_LEFT },
    OptStrIntMapping { "center", VELO_ANCHOR_CENTER },
    OptStrIntMapping { "right", VELO_ANCHOR_RIGHT },
};

// Names for ini.
OptStrIntMapping VELO_LENGTH_TABLE[] =
{
    OptStrIntMapping { "xy", VELO_LENGTH_XY },
    OptStrIntMapping { "xyz", VELO_LENGTH_XYZ },
    OptStrIntMapping { "z", VELO_LENGTH_Z },
};

// Names for ini.
// Should be synchronized with encoder_render.cpp.
const char* VIDEO_ENCODER_TABLE[] =
{
    "libx264",
    "libx264_444",
    "dnxhr",
};

// Names for ini.
// Should be synchronized with encoder_render.cpp.
const char* AUDIO_ENCODER_TABLE[] =
{
    "aac",
};

// Names for ini and ffmpeg.
const char* X264_PRESET_TABLE[] =
{
    "ultrafast",
    "superfast",
    "veryfast",
    "faster",
    "fast",
    "medium",
    "slow",
    "slower",
    "veryslow",
    "placebo",
};

// Names for ini.
// Should be synchronized with encoder_render.cpp.
const char* DNXHR_PROFILE_TABLE[] =
{
    "lb",
    "sq",
    "hq",
};

bool ProcState::movie_init()
{
    return true;
}

void ProcState::movie_free_static()
{
}

void ProcState::movie_free_dynamic()
{
}

bool ProcState::movie_start()
{
    return true;
}

void ProcState::movie_end()
{
}

void ProcState::movie_setup_params()
{
    D3D11_TEXTURE2D_DESC tex_desc;
    svr_game_texture.tex->GetDesc(&tex_desc);

    movie_width = tex_desc.Width;
    movie_height = tex_desc.Height;
}

// A required profile must have all variables set to a proper value. This is used with the default profile.
bool ProcState::movie_load_profile(const char* name, bool required)
{
    char full_profile_path[MAX_PATH];
    SVR_SNPRINTF(full_profile_path, "%s\\data\\profiles\\%s.ini", svr_resource_path, name);

    bool ret = false;

    SvrIniSection* ini_root = svr_ini_load(full_profile_path);

    if (ini_root == NULL)
    {
        game_log("ERROR: Could not load profile %s\n", full_profile_path);
        goto rfail;
    }

    ret = true;

    ret &= OPT_S32(ini_root, "video_fps", 1, 1000, &movie_profile.video_fps);
    ret &= OPT_STR_LIST(ini_root, "video_encoder", VIDEO_ENCODER_TABLE, &movie_profile.video_encoder);
    ret &= OPT_S32(ini_root, "video_x264_crf", 0, 52, &movie_profile.video_x264_crf);
    ret &= OPT_STR_LIST(ini_root, "video_x264_preset", X264_PRESET_TABLE, &movie_profile.video_x264_preset);
    ret &= OPT_BOOL(ini_root, "video_x264_intra", &movie_profile.video_x264_intra);
    ret &= OPT_STR_LIST(ini_root, "video_dnxhr_profile", DNXHR_PROFILE_TABLE, &movie_profile.video_dnxhr_profile);
    ret &= OPT_BOOL(ini_root, "audio_enabled", &movie_profile.audio_enabled);
    ret &= OPT_STR_LIST(ini_root, "audio_encoder", AUDIO_ENCODER_TABLE, &movie_profile.audio_encoder);

    ret &= OPT_BOOL(ini_root, "motion_blur_enabled", &movie_profile.mosample_enabled);
    ret &= OPT_S32(ini_root, "motion_blur_fps_mult", 2, INT32_MAX, &movie_profile.mosample_mult);
    ret &= OPT_FLOAT(ini_root, "motion_blur_exposure", 0.0f, 1.0f, &movie_profile.mosample_exposure);

    ret &= OPT_BOOL(ini_root, "velo_enabled", &movie_profile.velo_enabled);
    ret &= OPT_STR(ini_root, "velo_font", &movie_profile.velo_font);
    ret &= OPT_S32(ini_root, "velo_font_size", 16, 192, &movie_profile.velo_font_size);
    ret &= OPT_COLOR(ini_root, "velo_color", &movie_profile.velo_font_color);
    ret &= OPT_COLOR(ini_root, "velo_border_color", &movie_profile.velo_font_border_color);
    ret &= OPT_S32(ini_root, "velo_border_size", 0, 192, &movie_profile.velo_font_border_size);
    ret &= OPT_STR_MAP(ini_root, "velo_font_style", VELO_FONT_STYLE_TABLE, (s32*)&movie_profile.velo_font_style);
    ret &= OPT_STR_MAP(ini_root, "velo_font_weight", VELO_FONT_WEIGHT_TABLE, (s32*)&movie_profile.velo_font_weight);
    ret &= OPT_VEC2(ini_root, "velo_align", &movie_profile.velo_align);
    ret &= OPT_STR_MAP(ini_root, "velo_anchor", VELO_ANCHOR_TABLE, &movie_profile.velo_anchor);
    ret &= OPT_STR_MAP(ini_root, "velo_length", VELO_LENGTH_TABLE, &movie_profile.velo_length);

    if (!required)
    {
        ret = true;
    }

    goto rexit;

rfail:

rexit:
    if (ini_root)
    {
        svr_ini_free(ini_root);
    }

    return ret;
}
