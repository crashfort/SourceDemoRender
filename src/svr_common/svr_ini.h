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
void svr_ini_free_kv(SvrIniKeyValue* kv);
void svr_ini_free_kvs(SvrDynArray<SvrIniKeyValue*>* kvs);

// Find a keyvalue inside a section.
// Duplicate keyvalues are allowed, but this will only return the first.
// You can iterate over the kvs array if you need to handle duplicates.
SvrIniKeyValue* svr_ini_section_find_kv(SvrIniSection* priv, const char* key);

// Parse an INI style expression.
// Returns NULL if the expression could not be parsed.
// Result must be freed with svr_ini_free_kv.
SvrIniKeyValue* svr_ini_parse_expression(const char* expr);

// Parses an INI style command input.
// This is a series of INI style expressions on a single line.
// Places the results into dest.
// The destination array must be freed with svr_ini_free_kvs.
void svr_ini_parse_command_input(const char* input, SvrDynArray<SvrIniKeyValue*>* dest);

// Find the value of a key.
// Returns NULL if the key is not found.
const char* svr_ini_find_command_value(SvrDynArray<SvrIniKeyValue*>* kvs, const char* key);
