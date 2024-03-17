#pragma once
#include "svr_common.h"
#include "svr_array.h"

struct SvrVdfKeyValue
{
    char* key;
    char* value;
};

struct SvrVdfSection
{
    char* name;
    SvrDynArray<SvrVdfKeyValue*> kvs;
    SvrDynArray<SvrVdfSection*> sections;
};

// Load a VDF formatted file from a path.
SvrVdfSection* svr_vdf_load(const char* path);

// Call when no longer needed.
void svr_vdf_free(SvrVdfSection* priv);

// Checks if a section is the root section.
bool svr_vdf_section_is_root(SvrVdfSection* section);

// Find a nested section.
// A control index can be passed in to start searching from a given index.
// It is allowed to have several sections with the same name, so use the control index
// to iterate over sections with identical names.
SvrVdfSection* svr_vdf_section_find_section(SvrVdfSection* priv, const char* name, s32* control_idx);

// Find a keyvalue inside a section.
// The root section cannot contain keyvalues.
// Identical keyvalues are not allowed, so there is no control index here.
SvrVdfKeyValue* svr_vdf_section_find_kv(SvrVdfSection* priv, const char* key);

// Find a keyvalue from a section path.
// The last key should be the name of a keyvalue, while the previous keys should be the names of sections.
SvrVdfKeyValue* svr_vdf_section_find_kv_path(SvrVdfSection* priv, const char** keys, s32 num);

// Try to find a key and return the value, or return default if it doesn't exist.
const char* svr_vdf_section_find_value_or(SvrVdfSection* priv, const char* key, const char* def);
