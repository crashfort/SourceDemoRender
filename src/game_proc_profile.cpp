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
    StrIntMapping { "italic", DWRITE_FONT_STYLE_ITALIC },
    StrIntMapping { "extraitalic", DWRITE_FONT_STYLE_OBLIQUE },
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

// Names for ini.
const char* ENCODER_TABLE[] = {
    "libx264",
    "dnxhr",
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

// Names for ini.
const char* DNXHR_PROFILE_TABLE[] = {
    "lb",
    "sq",
    "hq",
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

void make_vec2(SvrIniLine* line, s32* target)
{
    s32 ret = sscanf(line->value, "%d %d", &target[0], &target[1]);

    if (ret != 2)
    {
        memset(target, 0, sizeof(s32) * 2);
        svr_log("Option %s has incorrect formatting. It should be in the format of <number> <number>. Setting to 0 0\n", line->title);
    }
}

void make_color(SvrIniLine* line, s32* target)
{
    s32 ret = sscanf(line->value, "%d %d %d", &target[0], &target[1], &target[2]);

    svr_clamp(&target[0], 0, 255);
    svr_clamp(&target[1], 0, 255);
    svr_clamp(&target[2], 0, 255);

    if (ret != 3)
    {
        memset(target, 255, sizeof(s32) * 3);
        svr_log("Option %s has incorrect formatting. It should be a color in the format of 255 255 255 (RGB). Setting to 255 255 255\n", line->title);
    }

    // Always opaque.
    target[3] = 255;
}

bool read_profile(const char* full_profile_path, MovieProfile* p)
{
    SvrIniMem ini_mem;

    if (!svr_open_ini_read(full_profile_path, &ini_mem))
    {
        game_log("Could not load profile %s\n", full_profile_path);
        return false;
    }

    SvrIniLine ini_line = svr_alloc_ini_line();
    SvrIniTokenType ini_token_type;

    #define OPT_S32(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atoi_in_range(&ini_line, MIN, MAX); }
    #define OPT_COLOR(NAME, VAR) (!strcmp(ini_line.title, NAME)) { make_color(&ini_line, VAR); }
    #define OPT_VEC2(NAME, VAR) (!strcmp(ini_line.title, NAME)) { make_vec2(&ini_line, VAR); }
    #define OPT_FLOAT(NAME, VAR, MIN, MAX) (!strcmp(ini_line.title, NAME)) { VAR = atof_in_range(&ini_line, MIN, MAX); }
    #define OPT_STR(NAME, VAR, SIZE) (!strcmp(ini_line.title, NAME)) { StringCchCopyA(VAR, SIZE, ini_line.value); }
    #define OPT_STR_LIST(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF); }
    #define OPT_STR_MAP(NAME, VAR, LIST, DEF) (!strcmp(ini_line.title, NAME)) { VAR = decltype(VAR)(map_str_in_list_or(&ini_line, LIST, SVR_ARRAY_SIZE(LIST), DEF)); }

    while (svr_read_ini(&ini_mem, &ini_line, &ini_token_type))
    {
        if OPT_S32("video_fps", p->movie_fps, 1, 1000)
        else if OPT_STR_LIST("video_encoder", p->sw_encoder, ENCODER_TABLE, "dnxhr")
        else if OPT_S32("video_x264_crf", p->sw_x264_crf, 0, 52)
        else if OPT_STR_LIST("video_x264_preset", p->sw_x264_preset, ENCODER_PRESET_TABLE, "veryfast")
        else if OPT_S32("video_x264_intra", p->sw_x264_intra, 0, 1)
        else if OPT_STR_LIST("video_dnxhr_profile", p->sw_dnxhr_profile, DNXHR_PROFILE_TABLE, "hq")
        else if OPT_S32("motion_blur_enabled", p->mosample_enabled, 0, 1)
        else if OPT_S32("motion_blur_fps_mult", p->mosample_mult, 2, INT32_MAX)
        else if OPT_FLOAT("motion_blur_exposure", p->mosample_exposure, 0.0f, 1.0f)
        else if OPT_S32("velo_enabled", p->veloc_enabled, 0, 1)
        else if OPT_STR("velo_font", p->veloc_font, MAX_VELOC_FONT_NAME)
        else if OPT_S32("velo_font_size", p->veloc_font_size, 16, 192)
        else if OPT_COLOR("velo_color", p->veloc_font_color)
        else if OPT_COLOR("velo_border_color", p->veloc_font_border_color)
        else if OPT_S32("velo_border_size", p->veloc_font_border_size, 0, 192)
        else if OPT_STR_MAP("velo_font_style", p->veloc_font_style, FONT_STYLE_TABLE, DWRITE_FONT_STYLE_NORMAL)
        else if OPT_STR_MAP("velo_font_weight", p->veloc_font_weight, FONT_WEIGHT_TABLE, DWRITE_FONT_WEIGHT_BOLD)
        else if OPT_VEC2("velo_align", p->veloc_align)
        else if OPT_S32("audio_enabled", p->audio_enabled, 0, 1)
    }

    svr_free_ini_line(&ini_line);
    svr_close_ini(&ini_mem);

    #undef OPT_S32
    #undef OPT_COLOR
    #undef OPT_VEC2
    #undef OPT_FLOAT
    #undef OPT_STR
    #undef OPT_STR_LIST
    #undef OPT_STR_MAP

    return true;
}
