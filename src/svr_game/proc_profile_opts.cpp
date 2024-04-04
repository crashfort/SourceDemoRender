#include "proc_priv.h"

bool opt_atoi_in_range(SvrIniKeyValue* kv, s32 min, s32 max, s32* dest)
{
    if (kv == NULL)
    {
        return false;
    }

    s32 v = atoi(kv->value);

    if (v < min || v > max)
    {
        s32 new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %d, max is %d, value is %d) setting to %d\n", kv->key, min, max, v, new_v);

        v = new_v;
    }

    *dest = v;
    return true;
}

bool opt_atof_in_range(SvrIniKeyValue* kv, float min, float max, float* dest)
{
    if (kv == NULL)
    {
        return false;
    }

    float v = atof(kv->value);

    if (v < min || v > max)
    {
        float new_v = v;
        svr_clamp(&new_v, min, max);

        game_log("Option %s out of range (min is %0.2f, max is %0.2f, value is %0.2f) setting to %0.2f\n", kv->key, min, max, v, new_v);

        v = new_v;
    }

    *dest = v;
    return true;
}

bool opt_str_or(SvrIniKeyValue* kv, char** dest)
{
    if (kv == NULL)
    {
        return false;
    }

    if (*dest)
    {
        svr_free(*dest);
        *dest = NULL;
    }

    *dest = svr_dup_str(kv->value);
    return true;
}

bool opt_str_in_list_or(SvrIniKeyValue* kv, const char** list, s32 num, const char** dest)
{
    if (kv == NULL)
    {
        return false;
    }

    for (s32 i = 0; i < num; i++)
    {
        if (!strcmp(list[i], kv->value))
        {
            *dest = list[i];
            return true;
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

    game_log("Option %s has incorrect value (value is %s, options are %s)\n", kv->key, kv->value, opts);

    return false;
}

bool opt_map_str_in_list_or(SvrIniKeyValue* kv, OptStrIntMapping* mappings, s32 num, s32* dest)
{
    if (kv == NULL)
    {
        return false;
    }

    for (s32 i = 0; i < num; i++)
    {
        OptStrIntMapping* m = &mappings[i];

        if (!strcmp(m->name, kv->value))
        {
            *dest = m->value;
            return true;
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

    game_log("Option %s has incorrect value (value is %s, options are %s)\n", kv->key, kv->value, opts);

    return false;
}

bool opt_make_vec2_or(SvrIniKeyValue* kv, SvrVec2I* dest)
{
    SvrVec2I ret;

    if (kv == NULL)
    {
        return false;
    }

    s32 num = sscanf(kv->value, "%d %d", &ret.x, &ret.y);

    if (num != 2)
    {
        ret = SvrVec2I { 0, 0 };
        game_log("Option %s has incorrect formatting. It should be in the format of <number> <number>. Setting to 0 0\n", kv->key);
    }

    *dest = ret;
    return true;
}

bool opt_make_color_or(SvrIniKeyValue* kv, SvrVec4I* dest)
{
    SvrVec4I ret;

    if (kv == NULL)
    {
        return false;
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

    *dest = ret;
    return true;
}
