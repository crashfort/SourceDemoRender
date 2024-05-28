#include "svr_ini.h"
#include "svr_alloc.h"
#include <strsafe.h>

using SvrIniLineType = s32;

enum /* SvrIniLineType */
{
    SVR_INI_LINE_NONE,
    SVR_INI_LINE_KV,
};

// Fast categorization of a line so we can parse it further.
SvrIniLineType svr_ini_categorize_line(const char* line)
{
    const char* ptr = svr_advance_until_after_whitespace(line);

    if (*ptr == 0)
    {
        return SVR_INI_LINE_NONE; // Blanks are no good.
    }

    if (*ptr == '#')
    {
        return SVR_INI_LINE_NONE; // Comments are no good.
    }

    if (svr_is_newline(ptr))
    {
        return SVR_INI_LINE_NONE; // Blanks are no good.
    }

    return SVR_INI_LINE_KV;
}

void svr_ini_parse_line(SvrIniSection* priv, const char* line, SvrIniLineType type)
{
    const char* ptr = svr_advance_until_after_whitespace(line); // Go past indentation.

    switch (type)
    {
        // Key values have two values.
        case SVR_INI_LINE_KV:
        {
            SvrIniKeyValue* kv = svr_ini_parse_expression(ptr);

            if (kv)
            {
                priv->kvs.push(kv);
            }

            break;
        }
    }
}

SvrIniSection* svr_ini_load(const char* path)
{
    char* file_mem = svr_read_file_as_string(path, 0);

    if (file_mem == NULL)
    {
        return NULL;
    }

    SvrIniSection* priv = SVR_ZALLOC(SvrIniSection);

    char line[8192];

    const char* prev_str = file_mem;

    while (true)
    {
        const char* next_str = svr_read_line(prev_str, line, SVR_ARRAY_SIZE(line));

        SvrIniLineType type = svr_ini_categorize_line(line);

        if (type != SVR_INI_LINE_NONE)
        {
            svr_ini_parse_line(priv, line, type);
        }

        prev_str = next_str;

        if (*next_str == 0)
        {
            break;
        }
    }

    svr_free(file_mem);

    return priv;
}

void svr_ini_free(SvrIniSection* priv)
{
    svr_ini_free_kvs(&priv->kvs);
    svr_free(priv);
}

void svr_ini_free_kv(SvrIniKeyValue* kv)
{
    svr_free(kv->key);
    svr_free(kv->value);
    svr_free(kv);
}

void svr_ini_free_kvs(SvrDynArray<SvrIniKeyValue*>* kvs)
{
    for (s32 i = 0; i < kvs->size; i++)
    {
        svr_ini_free_kv(kvs->at(i));
    }

    kvs->free();
}

SvrIniKeyValue* svr_ini_section_find_kv(SvrIniSection* priv, const char* key)
{
    for (s32 i = 0; i < priv->kvs.size; i++)
    {
        SvrIniKeyValue* kv = priv->kvs[i];

        if (!strcmpi(kv->key, key))
        {
            return kv;
        }
    }

    return NULL;
}

SvrIniKeyValue* svr_ini_parse_expression(const char* expr)
{
    // At most, one line can have a key and a value.
    char key_name[512];
    key_name[0] = 0;

    const char* ptr = svr_advance_until_after_whitespace(expr);

    const char* next_ptr = svr_advance_until_char(ptr, '='); // Read content.

    if (*next_ptr == 0)
    {
        return NULL; // There only a key.
    }

    s32 dist = next_ptr - ptr; // Content length.

    if (dist == 0)
    {
        return NULL; // There is only an equal sign and nothing else.
    }

    StringCchCopyNA(key_name, SVR_ARRAY_SIZE(key_name), ptr, dist);

    ptr = next_ptr;

    if (*ptr != '=')
    {
        return NULL; // There must not be any space before the equal sign.
    }

    ptr++; // Go past equal sign.

    if (*ptr == 0)
    {
        return NULL; // Value is missing.
    }

    next_ptr = svr_advance_until_after_whitespace(ptr); // Go past blanks.

    if (ptr != next_ptr)
    {
        return NULL; // There must not be a space after the equal sign.
    }

    SvrIniKeyValue* kv = SVR_ZALLOC(SvrIniKeyValue);
    kv->key = svr_dup_str(key_name);
    kv->value = svr_dup_str(ptr);

    return kv;
}

void svr_ini_parse_command_input(const char* input, SvrDynArray<SvrIniKeyValue*>* dest)
{
    const char* ptr = svr_advance_until_after_whitespace(input);

    while (*ptr != 0)
    {
        char expr[1024];
        expr[0] = 0;

        const char* next_ptr = svr_extract_string(ptr, expr, SVR_ARRAY_SIZE(expr));

        SvrIniKeyValue* kv = svr_ini_parse_expression(expr);

        if (kv)
        {
            dest->push(kv);
        }

        ptr = svr_advance_until_after_whitespace(next_ptr);
    }
}

const char* svr_ini_find_command_value(SvrDynArray<SvrIniKeyValue*>* kvs, const char* key)
{
    for (s32 i = 0; i < kvs->size; i++)
    {
        SvrIniKeyValue* kv = kvs->at(i);

        if (!strcmpi(kv->key, key))
        {
            return kv->value;
        }
    }

    return NULL;
}
