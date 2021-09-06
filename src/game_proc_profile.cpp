#include "game_proc_profile.h"
#include "game_shared.h"
#include "svr_ini.h"
#include <strsafe.h>
#include <dwrite.h>

struct StrIntMapping
{
    const char* name;
    s32 value;
};

// Font stuff for velo.

// Names for ini.
StrIntMapping FONT_WEIGHT_TABLE[] = {
    StrIntMapping { "thin", DWRITE_FONT_WEIGHT_THIN },
    StrIntMapping { "extralight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT },
    StrIntMapping { "light", DWRITE_FONT_WEIGHT_LIGHT },
    StrIntMapping { "semilight", DWRITE_FONT_WEIGHT_SEMI_LIGHT },
    StrIntMapping { "normal", DWRITE_FONT_WEIGHT_NORMAL },
    StrIntMapping { "medium", DWRITE_FONT_WEIGHT_MEDIUM },
    StrIntMapping { "semibold", DWRITE_FONT_WEIGHT_SEMI_BOLD },
    StrIntMapping { "bold", DWRITE_FONT_WEIGHT_BOLD },
    StrIntMapping { "extrabold", DWRITE_FONT_WEIGHT_EXTRA_BOLD },
    StrIntMapping { "black", DWRITE_FONT_WEIGHT_BLACK },
    StrIntMapping { "extrablack", DWRITE_FONT_WEIGHT_EXTRA_BLACK },
};

// Names for ini.
StrIntMapping FONT_STYLE_TABLE[] = {
    StrIntMapping { "normal", DWRITE_FONT_STYLE_NORMAL },
    StrIntMapping { "oblique", DWRITE_FONT_STYLE_OBLIQUE },
    StrIntMapping { "italic", DWRITE_FONT_STYLE_ITALIC },
};

// Names for ini.
StrIntMapping FONT_STRETCH_TABLE[] = {
    StrIntMapping { "undefined", DWRITE_FONT_STRETCH_UNDEFINED },
    StrIntMapping { "ultracondensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED },
    StrIntMapping { "extracondensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED },
    StrIntMapping { "condensed", DWRITE_FONT_STRETCH_CONDENSED },
    StrIntMapping { "semicondensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED },
    StrIntMapping { "normal", DWRITE_FONT_STRETCH_NORMAL },
    StrIntMapping { "semiexpanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED },
    StrIntMapping { "expanded", DWRITE_FONT_STRETCH_EXPANDED },
    StrIntMapping { "extraexpanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED },
    StrIntMapping { "ultraexpanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED },
};

// Names for ini.
StrIntMapping TEXT_ALIGN_TABLE[] = {
    StrIntMapping { "leading", DWRITE_TEXT_ALIGNMENT_LEADING },
    StrIntMapping { "trailing", DWRITE_TEXT_ALIGNMENT_TRAILING },
    StrIntMapping { "center", DWRITE_TEXT_ALIGNMENT_CENTER },
};

// Names for ini.
StrIntMapping PARAGRAPH_ALIGN_TABLE[] = {
    StrIntMapping { "near", DWRITE_PARAGRAPH_ALIGNMENT_NEAR },
    StrIntMapping { "far", DWRITE_PARAGRAPH_ALIGNMENT_FAR },
    StrIntMapping { "center", DWRITE_PARAGRAPH_ALIGNMENT_CENTER },
};

// Names for ini.
const char* PXFORMAT_TABLE[] = {
    "yuv420",
    "yuv444",
    "nv12",
    "nv21",
    "bgr0",
};

// Names for ini.
const char* COLORSPACE_TABLE[] = {
    "601",
    "709",
    "rgb",
};

// Names for ini and ffmpeg.
const char* ENCODER_TABLE[] = {
    "libx264",
    "libx264rgb",
};

// Names for ini and ffmpeg.
const char* ENCODER_PRESET_TABLE[] = {
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

s32 atoi_in_range(SvrIniLine* line, s32 min, s32 max)
{
    s32 v = strtol(line->value, NULL, 10);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(&new_v, min, max);

        svr_log("Option %s out of range (min is %d, max is %d, value is %d) setting to %d\n", line->title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

float atof_in_range(SvrIniLine* line, float min, float max)
{
    float v = atof(line->value);

    if (v < min || v > max)
    {
        float new_v = v;
        svr_clamp(&new_v, min, max);

        svr_log("Option %s out of range (min is %0.2f, max is %0.2f, value is %0.2f) setting to %0.2f\n", line->title, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

const char* str_in_list_or(SvrIniLine* line, const char** list, s32 num, const char* def)
{
    for (s32 i = 0; i < num; i++)
    {
        if (!strcmp(list[i], line->value))
        {
            return list[i];
        }
    }

    const s32 OPTS_SIZE = 1024;

    char opts[OPTS_SIZE];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        StringCchCatA(opts, OPTS_SIZE, list[i]);

        if (i != num - 1)
        {
            StringCchCatA(opts, OPTS_SIZE, ", ");
        }
    }

    svr_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line->title, line->value, opts, def);

    return def;
}

const char* rl_map_str_in_list(s32 value, StrIntMapping* mappings, s32 num)
{
    for (s32 i = 0; i < num; i++)
    {
        StrIntMapping& m = mappings[i];

        if (m.value == value)
        {
            return m.name;
        }
    }

    return NULL;
}

s32 map_str_in_list_or(SvrIniLine* line, StrIntMapping* mappings, s32 num, s32 def)
{
    for (s32 i = 0; i < num; i++)
    {
        StrIntMapping& m = mappings[i];

        if (!strcmp(m.name, line->value))
        {
            return m.value;
        }
    }

    const s32 OPTS_SIZE = 1024;

    char opts[OPTS_SIZE];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        StringCchCatA(opts, OPTS_SIZE, mappings[i].name);

        if (i != num - 1)
        {
            StringCchCatA(opts, OPTS_SIZE, ", ");
        }
    }

    const char* def_title = rl_map_str_in_list(def, mappings, num);

    svr_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", line->title, line->value, opts, def_title);

    return def;
}

bool read_profile(const char* full_profile_path, MovieProfile* p)
{
    SvrIniMem ini_mem;

    if (!svr_open_ini_read(full_profile_path, &ini_mem))
    {
        return false;
    }

    SvrIniLine ini_line = svr_alloc_ini_line();
    SvrIniTokenType ini_token_type;

    #define OPT_S32(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atoi_in_range(&ini_line, MIN, MAX); }
    #define OPT_FLOAT(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atof_in_range(&ini_line, MIN, MAX); }
    #define OPT_STR(NAME, VAR, SIZE) (!strcmp(ini_line.title, NAME)) { StringCchCopyA(VAR, SIZE, ini_line.value); }
    #define OPT_STR_LIST(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF); }
    #define OPT_STR_MAP(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = decltype(VAR)(map_str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF)); }

    while (svr_read_ini(&ini_mem, &ini_line, &ini_token_type))
    {
        if OPT_S32("video_fps", p->movie_fps, 1, 1000)
        else if OPT_STR_LIST("video_encoder", p->sw_encoder, ENCODER_TABLE, "libx264")
        else if OPT_STR_LIST("video_pixel_format", p->sw_pxformat, PXFORMAT_TABLE, "yuv420")
        else if OPT_STR_LIST("video_colorspace", p->sw_colorspace, COLORSPACE_TABLE, "601")
        else if OPT_S32("video_x264_crf", p->sw_crf, 0, 52)
        else if OPT_STR_LIST("video_x264_preset", p->sw_x264_preset, ENCODER_PRESET_TABLE, "veryfast")
        else if OPT_S32("video_x264_intra", p->sw_x264_intra, 0, 1)
        else if OPT_S32("motion_blur_enabled", p->mosample_enabled, 0, 1)
        else if OPT_S32("motion_blur_fps_mult", p->mosample_mult, 1, INT32_MAX)
        else if OPT_FLOAT("motion_blur_frame_exposure", p->mosample_exposure, 0.0f, 1.0f)
        else if OPT_S32("velocity_overlay_enabled", p->veloc_enabled, 0, 1)
        else if OPT_STR("velocity_overlay_font_family", p->veloc_font, MAX_VELOC_FONT_NAME)
        else if OPT_S32("velocity_overlay_font_size", p->veloc_font_size, 0, INT32_MAX)
        else if OPT_S32("velocity_overlay_color_r", p->veloc_font_color[0], 0, 255)
        else if OPT_S32("velocity_overlay_color_g", p->veloc_font_color[1], 0, 255)
        else if OPT_S32("velocity_overlay_color_b", p->veloc_font_color[2], 0, 255)
        else if OPT_S32("velocity_overlay_color_a", p->veloc_font_color[3], 0, 255)
        else if OPT_STR_MAP("velocity_overlay_font_style", p->veloc_font_style, FONT_STYLE_TABLE, DWRITE_FONT_STYLE_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_font_weight", p->veloc_font_weight, FONT_WEIGHT_TABLE, DWRITE_FONT_WEIGHT_BOLD)
        else if OPT_STR_MAP("velocity_overlay_font_stretch", p->veloc_font_stretch, FONT_STRETCH_TABLE, DWRITE_FONT_STRETCH_NORMAL)
        else if OPT_STR_MAP("velocity_overlay_text_align", p->veloc_text_align, TEXT_ALIGN_TABLE, DWRITE_TEXT_ALIGNMENT_CENTER)
        else if OPT_STR_MAP("velocity_overlay_paragraph_align", p->veloc_para_align, PARAGRAPH_ALIGN_TABLE, DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
        else if OPT_S32("velocity_overlay_padding", p->veloc_padding, 0, INT32_MAX)
    }

    svr_free_ini_line(&ini_line);
    svr_close_ini(&ini_mem);

    #undef OPT_S32
    #undef OPT_FLOAT
    #undef OPT_STR
    #undef OPT_STR_LIST
    #undef OPT_STR_MAP

    return true;
}

bool verify_profile(MovieProfile* p)
{
    // We discard the movie if these are not right, because we don't want to spend
    // a very long time creating a movie which then would get thrown away since it didn't use the correct settings.

    if (!strcmp(p->sw_encoder, "libx264"))
    {
        if (!strcmp(p->sw_pxformat, "bgr0"))
        {
            game_log("The libx264 encoder can only use YUV pixel formats\n");
            return false;
        }

        if (!strcmp(p->sw_colorspace, "rgb"))
        {
            game_log("The libx264 encoder can only use YUV color spaces\n");
            return false;
        }
    }

    else if (!strcmp(p->sw_encoder, "libx264rgb"))
    {
        if (strcmp(p->sw_pxformat, "bgr0"))
        {
            game_log("The libx264rgb encoder can only use the rgb pixel format\n");
            return false;
        }

        if (strcmp(p->sw_colorspace, "rgb"))
        {
            game_log("The libx264rgb encoder can only use the rgb color space\n");
            return false;
        }
    }

    if (p->mosample_mult == 1)
    {
        game_log("motion_blur_fps_mult is set to 1, which doesn't enable motion blur\n");
        return false;
    }

    return true;
}

void set_default_profile(MovieProfile* p)
{
    p->movie_fps = 60;
    p->sw_encoder = "libx264";
    p->sw_pxformat = "yuv420";
    p->sw_colorspace = "601";
    p->sw_crf = 23;
    p->sw_x264_preset = "veryfast";
    p->sw_x264_intra = 0;
    p->mosample_enabled = 1;
    p->mosample_mult = 60;
    p->mosample_exposure = 0.5f;
    p->veloc_enabled = 0;
    StringCchCopyA(p->veloc_font, MAX_VELOC_FONT_NAME, "Arial");
    p->veloc_font_size = 72;
    p->veloc_font_color[0] = 255;
    p->veloc_font_color[1] = 255;
    p->veloc_font_color[2] = 255;
    p->veloc_font_color[3] = 255;
    p->veloc_font_style = DWRITE_FONT_STYLE_NORMAL;
    p->veloc_font_weight = DWRITE_FONT_WEIGHT_BOLD;
    p->veloc_font_stretch = DWRITE_FONT_STRETCH_NORMAL;
    p->veloc_text_align = DWRITE_TEXT_ALIGNMENT_CENTER;
    p->veloc_para_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    p->veloc_padding = 100;
}

void log_profile(MovieProfile* p)
{
    svr_log("Movie fps: %d\n", p->movie_fps);
    svr_log("Video encoder: %s\n", p->sw_encoder);
    svr_log("Video pixel format: %s\n", p->sw_pxformat);
    svr_log("Video colorspace: %s\n", p->sw_colorspace);
    svr_log("Video crf: %d\n", p->sw_crf);
    svr_log("Video x264 preset: %s\n", p->sw_x264_preset);
    svr_log("Video x264 intra: %d\n", p->sw_x264_intra);
    svr_log("Use motion blur: %d\n", p->mosample_enabled);

    if (p->mosample_enabled)
    {
        svr_log("Motion blur multiplier: %d\n", p->mosample_mult);
        svr_log("Motion blur exposure: %0.2f\n", p->mosample_exposure);
    }

    svr_log("Use velocity overlay: %d\n", p->veloc_enabled);

    if (p->veloc_enabled)
    {
        svr_log("Velocity font: %s\n", p->veloc_font);
        svr_log("Velocity font size: %d\n", p->veloc_font_size);
        svr_log("Velocity font color: %d %d %d %d\n", p->veloc_font_color[0], p->veloc_font_color[1], p->veloc_font_color[2], p->veloc_font_color[3]);
        svr_log("Velocity font style: %s\n", rl_map_str_in_list(p->veloc_font_style, FONT_STYLE_TABLE, SVR_ARRAY_SIZE(FONT_STYLE_TABLE)));
        svr_log("Velocity font weight: %s\n", rl_map_str_in_list(p->veloc_font_weight, FONT_WEIGHT_TABLE, SVR_ARRAY_SIZE(FONT_WEIGHT_TABLE)));
        svr_log("Velocity font stretch: %s\n", rl_map_str_in_list(p->veloc_font_stretch, FONT_STRETCH_TABLE, SVR_ARRAY_SIZE(FONT_STRETCH_TABLE)));
        svr_log("Velocity text align: %s\n", rl_map_str_in_list(p->veloc_text_align, TEXT_ALIGN_TABLE, SVR_ARRAY_SIZE(TEXT_ALIGN_TABLE)));
        svr_log("Velocity paragraph align: %s\n", rl_map_str_in_list(p->veloc_para_align, PARAGRAPH_ALIGN_TABLE, SVR_ARRAY_SIZE(PARAGRAPH_ALIGN_TABLE)));
        svr_log("Velocity text padding: %d\n", p->veloc_padding);
    }
}
