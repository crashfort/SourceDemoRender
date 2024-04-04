#pragma once

// Stuff to make it easier to read from the profile.

struct OptStrIntMapping
{
    const char* name;
    s32 value;
};

bool opt_atoi_in_range(SvrIniKeyValue* kv, s32 min, s32 max, s32* dest);
bool opt_atof_in_range(SvrIniKeyValue* kv, float min, float max, float* dest);
bool opt_str_or(SvrIniKeyValue* kv, char** dest);
bool opt_str_in_list_or(SvrIniKeyValue* kv, const char** list, s32 num, const char** dest);
bool opt_map_str_in_list_or(SvrIniKeyValue* kv, OptStrIntMapping* mappings, s32 num, s32* dest);
bool opt_make_vec2_or(SvrIniKeyValue* kv, SvrVec2I* dest);
bool opt_make_color_or(SvrIniKeyValue* kv, SvrVec4I* dest);

#define OPT_S32(INI, NAME, MIN, MAX, DEST) opt_atoi_in_range(svr_ini_section_find_kv(INI, NAME), MIN, MAX, DEST)
#define OPT_FLOAT(INI, NAME, MIN, MAX, DEST) opt_atof_in_range(svr_ini_section_find_kv(INI, NAME), MIN, MAX, DEST)
#define OPT_BOOL(INI, NAME, DEST) opt_atoi_in_range(svr_ini_section_find_kv(INI, NAME), 0, 1, DEST)
#define OPT_STR(INI, NAME, DEST) opt_str_or(svr_ini_section_find_kv(INI, NAME), DEST)
#define OPT_COLOR(INI, NAME, DEST) opt_make_color_or(svr_ini_section_find_kv(INI, NAME), DEST)
#define OPT_VEC2(INI, NAME, DEST) opt_make_vec2_or(svr_ini_section_find_kv(INI, NAME), DEST)
#define OPT_STR_LIST(INI, NAME, LIST, DEST) opt_str_in_list_or(svr_ini_section_find_kv(INI, NAME), LIST, SVR_ARRAY_SIZE(LIST), DEST)
#define OPT_STR_MAP(INI, NAME, MAP, DEST) opt_map_str_in_list_or(svr_ini_section_find_kv(INI, NAME), MAP, SVR_ARRAY_SIZE(MAP), DEST)
