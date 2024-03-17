#include "proc_priv.h"

s32 opt_atoi_in_range(SvrIniKeyValue* kv, s32 min, s32 max, s32 def)
{
    if (kv == NULL)
    {
        return def;
    }

    s32 v = atoi(kv->value);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %d, max is %d, value is %d) setting to %d\n", kv->key, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

float opt_atof_in_range(SvrIniKeyValue* kv, float min, float max, float def)
{
    if (kv == NULL)
    {
        return def;
    }

    float v = atof(kv->value);

    if (v < min || v > max)
    {
        float new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %0.2f, max is %0.2f, value is %0.2f) setting to %0.2f\n", kv->key, min, max, v, new_v);

        v = new_v;
    }

    return v;
}

char* opt_str_or(SvrIniKeyValue* kv, const char* def)
{
    if (kv == NULL)
    {
        return svr_dup_str(def);
    }

    return svr_dup_str(kv->value);
}

const char* opt_str_in_list_or(SvrIniKeyValue* kv, const char** list, s32 num, const char* def)
{
    if (kv == NULL)
    {
        return def;
    }

    for (s32 i = 0; i < num; i++)
    {
        if (!strcmp(list[i], kv->value))
        {
            return list[i];
        }
    }

    char opts[1024];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        StringCchCatA(opts, SVR_ARRAY_SIZE(opts), list[i]);

        if (i != num - 1)
        {
            StringCchCatA(opts, SVR_ARRAY_SIZE(opts), ", ");
        }
    }

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", kv->key, kv->value, opts, def);

    return def;
}

const char* opt_rl_map_str_in_list(s32 value, OptStrIntMapping* mappings, s32 num)
{
    for (s32 i = 0; i < num; i++)
    {
        OptStrIntMapping* m = &mappings[i];

        if (m->value == value)
        {
            return m->name;
        }
    }

    return NULL;
}

s32 opt_map_str_in_list_or(SvrIniKeyValue* kv, OptStrIntMapping* mappings, s32 num, s32 def)
{
    if (kv == NULL)
    {
        OptStrIntMapping* m = &mappings[def];
        return m->value;
    }

    for (s32 i = 0; i < num; i++)
    {
        OptStrIntMapping* m = &mappings[i];

        if (!strcmp(m->name, kv->value))
        {
            return m->value;
        }
    }

    char opts[1024];
    opts[0] = 0;

    for (s32 i = 0; i < num; i++)
    {
        OptStrIntMapping* m = &mappings[i];
        StringCchCatA(opts, SVR_ARRAY_SIZE(opts), m->name);

        if (i != num - 1)
        {
            StringCchCatA(opts, SVR_ARRAY_SIZE(opts), ", ");
        }
    }

    const char* def_title = opt_rl_map_str_in_list(def, mappings, num);

    game_log("Option %s has incorrect value (value is %s, options are %s) setting to %s\n", kv->key, kv->value, opts, def_title);

    return def;
}

SvrVec2I opt_make_vec2_or(SvrIniKeyValue* kv, SvrVec2I def)
{
    SvrVec2I ret;

    if (kv == NULL)
    {
        return def;
    }

    s32 num = sscanf(kv->value, "%d %d", &ret.x, &ret.y);

    if (num != 2)
    {
        ret = SvrVec2I { 0, 0 };
        game_log("Option %s has incorrect formatting. It should be in the format of <number> <number>. Setting to 0 0\n", kv->key);
    }

    return ret;
}

SvrVec4I opt_make_color_or(SvrIniKeyValue* kv, SvrVec4I def)
{
    SvrVec4I ret;

    if (kv == NULL)
    {
        return def;
    }

    s32 num = sscanf(kv->value, "%d %d %d %d", &ret.x, &ret.y, &ret.z, &ret.w);

    svr_clamp(&ret.x, 0, 255);
    svr_clamp(&ret.y, 0, 255);
    svr_clamp(&ret.z, 0, 255);
    svr_clamp(&ret.w, 0, 255);

    if (num != 4)
    {
        ret = SvrVec4I { 255, 255, 255, 255 };
        game_log("Option %s has incorrect formatting. It should be a color in the format of 255 255 255 255 (RGBA). Setting to 255 255 255 255\n", kv->key);
    }

    return ret;
}
