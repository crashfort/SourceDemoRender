#pragma once

// Stuff to make it easier to read from the profile.

struct OptStrIntMapping
{
    const char* name;
    s32 value;
};

s32 opt_atoi_in_range(SvrIniKeyValue* kv, s32 min, s32 max, s32 def);
float opt_atof_in_range(SvrIniKeyValue* kv, float min, float max, float def);
char* opt_str_or(SvrIniKeyValue* kv, const char* def);
const char* opt_str_in_list_or(SvrIniKeyValue* kv, const char** list, s32 num, const char* def);
const char* opt_rl_map_str_in_list(s32 value, OptStrIntMapping* mappings, s32 num);
s32 opt_map_str_in_list_or(SvrIniKeyValue* kv, OptStrIntMapping* mappings, s32 num, s32 def);
SvrVec2I opt_make_vec2_or(SvrIniKeyValue* kv, SvrVec2I def);
SvrVec4I opt_make_color_or(SvrIniKeyValue* kv, SvrVec4I def);

#define OPT_S32(INI, NAME, MIN, MAX, ...) opt_atoi_in_range(svr_ini_section_find_kv(INI, NAME), MIN, MAX, __VA_ARGS__)
#define OPT_FLOAT(INI, NAME, MIN, MAX, ...) opt_atof_in_range(svr_ini_section_find_kv(INI, NAME), MIN, MAX, __VA_ARGS__)
#define OPT_BOOL(INI, NAME, ...) opt_atoi_in_range(svr_ini_section_find_kv(INI, NAME), 0, 1, __VA_ARGS__)
#define OPT_STR(INI, NAME, ...) opt_str_or(svr_ini_section_find_kv(INI, NAME), __VA_ARGS__)
#define OPT_COLOR(INI, NAME, ...) opt_make_color_or(svr_ini_section_find_kv(INI, NAME), __VA_ARGS__)
#define OPT_VEC2(INI, NAME, ...) opt_make_vec2_or(svr_ini_section_find_kv(INI, NAME), __VA_ARGS__)
#define OPT_STR_LIST(INI, NAME, LIST, ...) opt_str_in_list_or(svr_ini_section_find_kv(INI, NAME), LIST, SVR_ARRAY_SIZE(LIST), __VA_ARGS__)
#define OPT_STR_MAP(INI, NAME, MAP, ...) opt_map_str_in_list_or(svr_ini_section_find_kv(INI, NAME), MAP, SVR_ARRAY_SIZE(MAP), __VA_ARGS__)
