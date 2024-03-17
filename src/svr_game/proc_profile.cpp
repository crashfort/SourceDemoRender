#include "proc_priv.h"

// Font stuff for velo.

// Names for ini.
OptStrIntMapping VELO_FONT_WEIGHT_TABLE[] = {
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
OptStrIntMapping VELO_FONT_STYLE_TABLE[] = {
    OptStrIntMapping { "normal", DWRITE_FONT_STYLE_NORMAL },
    OptStrIntMapping { "italic", DWRITE_FONT_STYLE_ITALIC },
    OptStrIntMapping { "extraitalic", DWRITE_FONT_STYLE_OBLIQUE },
};

// Names for ini.
// Should be synchronized with encoder_render.cpp.
const char* VIDEO_ENCODER_TABLE[] = {
    "libx264",
    "dnxhr",
};

// Names for ini.
// Should be synchronized with encoder_render.cpp.
const char* AUDIO_ENCODER_TABLE[] = {
    "aac",
};

// Names for ini and ffmpeg.
const char* X264_PRESET_TABLE[] = {
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
const char* DNXHR_PROFILE_TABLE[] = {
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

bool ProcState::movie_load_profile(const char* profile_path)
{
    bool ret = false;

    movie_free_profile();

    SvrIniSection* ini_root = svr_ini_load(profile_path);

    if (ini_root == NULL)
    {
        game_log("ERROR: Could not load profile %s\n", profile_path);
        goto rfail;
    }

    movie_profile.video_fps = OPT_S32(ini_root, "video_fps", 1, 1000, 60);
    movie_profile.video_encoder = OPT_STR_LIST(ini_root, "video_encoder", VIDEO_ENCODER_TABLE, "dnxhr");
    movie_profile.video_x264_crf = OPT_S32(ini_root, "video_x264_crf", 0, 52, 15);
    movie_profile.video_x264_preset = OPT_STR_LIST(ini_root, "video_x264_preset", X264_PRESET_TABLE, "veryfast");
    movie_profile.video_x264_intra = OPT_BOOL(ini_root, "video_x264_intra", 0);
    movie_profile.video_dnxhr_profile = OPT_STR_LIST(ini_root, "video_dnxhr_profile", DNXHR_PROFILE_TABLE, "hq");
    movie_profile.audio_enabled = OPT_BOOL(ini_root, "audio_enabled", 0);
    movie_profile.audio_encoder = OPT_STR_LIST(ini_root, "audio_encoder", AUDIO_ENCODER_TABLE, "aac");

    movie_profile.mosample_enabled = OPT_BOOL(ini_root, "motion_blur_enabled", 0);
    movie_profile.mosample_mult = OPT_S32(ini_root, "motion_blur_fps_mult", 2, INT32_MAX, 60);
    movie_profile.mosample_exposure = OPT_FLOAT(ini_root, "motion_blur_exposure", 0.0f, 1.0f, 0.5f);

    movie_profile.velo_enabled = OPT_BOOL(ini_root, "velo_enabled", 0);
    movie_profile.velo_font = OPT_STR(ini_root, "velo_font", "Segoe UI");
    movie_profile.velo_font_size = OPT_S32(ini_root, "velo_font_size", 16, 192, 48);
    movie_profile.velo_font_color = OPT_COLOR(ini_root, "velo_color", SvrVec4I { 255, 255, 255, 100 });
    movie_profile.velo_font_border_color = OPT_COLOR(ini_root, "velo_border_color", SvrVec4I { 0, 0, 0, 255 });
    movie_profile.velo_font_border_size = OPT_S32(ini_root, "velo_border_size", 0, 192, 0);
    movie_profile.velo_font_style = (DWRITE_FONT_STYLE)OPT_STR_MAP(ini_root, "velo_font_style", VELO_FONT_STYLE_TABLE, DWRITE_FONT_STYLE_NORMAL);
    movie_profile.velo_font_weight = (DWRITE_FONT_WEIGHT)OPT_STR_MAP(ini_root, "velo_font_weight", VELO_FONT_WEIGHT_TABLE, DWRITE_FONT_WEIGHT_NORMAL);
    movie_profile.velo_align = OPT_VEC2(ini_root, "velo_align", SvrVec2I { 0, 80 });

    ret = true;
    goto rexit;

rfail:

rexit:
    if (ini_root)
    {
        svr_ini_free(ini_root);
    }

    return ret;
}

void ProcState::movie_free_profile()
{
    if (movie_profile.velo_font)
    {
        svr_free(movie_profile.velo_font);
        movie_profile.velo_font = NULL;
    }
}
