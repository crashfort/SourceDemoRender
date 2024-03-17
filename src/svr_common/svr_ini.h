#pragma once
#include "svr_common.h"
#include "svr_array.h"

// We use ini now instead of json for two reasons: First, json is overly complicated to parse and libraries are overly complicated. Second, users get confused with the formatting rules
// and cases that include escaping a sequence of characters.

// This only supports a flat structure of keyvalues. Sections are not supported.

struct SvrIniKeyValue
{
    char* key;
    char* value;
};

struct SvrIniSection
{
    SvrDynArray<SvrIniKeyValue*> kvs;
};

SvrIniSection* svr_ini_load(const char* path);

void svr_ini_free(SvrIniSection* priv);

// Find a keyvalue inside a section.
// Duplicate keyvalues are allowed, but this will only return the first.
// You can iterate over the kvs array if you need to handle duplicates.
SvrIniKeyValue* svr_ini_section_find_kv(SvrIniSection* priv, const char* key);
