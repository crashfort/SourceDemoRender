#include "svr_ini.h"
#include "svr_alloc.h"

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
    // At most, one line can have a key and a value.
    char key_name[512];
    key_name[0] = 0;

    const char* ptr = svr_advance_until_after_whitespace(line); // Go past indentation.

    switch (type)
    {
        // Key values have two values.
        case SVR_INI_LINE_KV:
        {
            const char* next_ptr = svr_advance_until_char(ptr, '='); // Read content.
            s32 dist = next_ptr - ptr; // Content length.
            strncat(key_name, ptr, svr_min(dist, SVR_ARRAY_SIZE(key_name) - 1));

            ptr = next_ptr;
            ptr++; // Go past equal sign.

            ptr = svr_advance_until_after_whitespace(ptr); // Go past blanks.

            auto kv = SVR_ZALLOC(SvrIniKeyValue);
            kv->key = svr_dup_str(key_name);
            kv->value = svr_dup_str(ptr);

            priv->kvs.push(kv);
            break;
        }
    }
}

SvrIniSection* svr_ini_load(const char* path)
{
    char* file_mem = svr_read_file_as_string(path);

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
    for (s32 i = 0; i < priv->kvs.size; i++)
    {
        SvrIniKeyValue* kv = priv->kvs[i];
        svr_free(kv->key);
        svr_free(kv->value);
        svr_free(kv);
    }

    priv->kvs.free();

    svr_free(priv);
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
