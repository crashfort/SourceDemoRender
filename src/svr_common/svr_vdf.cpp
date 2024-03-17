#include "svr_vdf.h"
#include "svr_alloc.h"

using SvrVdfLineType = s32;

enum /* SvrVdfLineType */
{
    SVR_VDF_LINE_NONE,
    SVR_VDF_LINE_SECTION_NAME,
    SVR_VDF_LINE_SECTION_START,
    SVR_VDF_LINE_SECTION_END,
    SVR_VDF_LINE_KV,
};

struct SvrVdfParseState
{
    SvrDynArray<SvrVdfSection*> section_stack;
};

// Fast categorization of a line so we can parse it further.
SvrVdfLineType svr_vdf_categorize_line(const char* line)
{
    const char* ptr = line;
    ptr = svr_advance_until_after_whitespace(ptr);

    if (*ptr == '{')
    {
        return SVR_VDF_LINE_SECTION_START;
    }

    if (*ptr == '}')
    {
        return SVR_VDF_LINE_SECTION_END;
    }

    if (*ptr == '/')
    {
        return SVR_VDF_LINE_NONE; // Comments are no good.
    }

    if (svr_is_newline(ptr))
    {
        return SVR_VDF_LINE_NONE; // Blanks are no good.
    }

    // Section starts only have a key and not a value.
    // They may or may not be in quotes.

    const char* next_ptr = svr_advance_until_whitespace(ptr); // Just see if there is more after.

    if (*next_ptr == 0)
    {
        return SVR_VDF_LINE_SECTION_NAME; // Nothing more after, must be a section name.
    }

    // Key values have two values.
    // Both the key and the value may or may not be in quotes.

    return SVR_VDF_LINE_KV;
}

void svr_vdf_kv_free(SvrVdfKeyValue* priv)
{
    if (priv->key)
    {
        svr_free(priv->key);
        priv->key = NULL;
    }

    if (priv->value)
    {
        svr_free(priv->value);
        priv->value = NULL;
    }
}

void svr_vdf_section_free(SvrVdfSection* priv)
{
    for (s32 i = 0; i < priv->kvs.size; i++)
    {
        SvrVdfKeyValue* k = priv->kvs[i];
        svr_vdf_kv_free(k);
        svr_free(k);
    }

    for (s32 i = 0; i < priv->sections.size; i++)
    {
        SvrVdfSection* s = priv->sections[i];
        svr_vdf_section_free(s);
        svr_free(s);
    }

    if (priv->name)
    {
        svr_free(priv->name);
        priv->name = NULL;
    }

    priv->kvs.free();
    priv->sections.free();
}

void svr_vdf_section_add_kv(SvrVdfSection* priv, const char* key, const char* value)
{
    SvrVdfKeyValue* kv = SVR_ZALLOC(SvrVdfKeyValue);
    kv->key = svr_dup_str(key);
    kv->value = svr_dup_str(value);

    priv->kvs.push(kv);
}

SvrVdfSection* svr_vdf_section_add_section(SvrVdfSection* priv, const char* name)
{
    SvrVdfSection* section = SVR_ZALLOC(SvrVdfSection);
    section->name = svr_dup_str(name);

    priv->sections.push(section);

    return section;
}

SvrVdfSection* svr_vdf_section_find_section(SvrVdfSection* priv, const char* name, s32* control_idx)
{
    s32 idx = 0;

    if (control_idx)
    {
        idx = *control_idx;
    }

    for (s32 i = idx; i < priv->sections.size; i++)
    {
        SvrVdfSection* s = priv->sections[i];

        if (!strcmpi(s->name, name))
        {
            if (control_idx)
            {
                *control_idx = i + 1; // Next index should be past this one.
            }

            return s;
        }
    }

    return NULL;
}

SvrVdfKeyValue* svr_vdf_section_find_kv(SvrVdfSection* priv, const char* key)
{
    for (s32 i = 0; i < priv->kvs.size; i++)
    {
        SvrVdfKeyValue* k = priv->kvs[i];

        if (!strcmpi(k->key, key))
        {
            return k;
        }
    }

    return NULL;
}

SvrVdfKeyValue* svr_vdf_section_find_kv_path(SvrVdfSection* priv, const char** keys, s32 num)
{
    SvrVdfSection* from = priv;

    for (s32 i = 0; i < num - 1; i++)
    {
        from = svr_vdf_section_find_section(from, keys[i], NULL);

        if (from == NULL)
        {
            break;
        }
    }

    if (from)
    {
        SvrVdfKeyValue* kv = svr_vdf_section_find_kv(from, keys[num - 1]);
        return kv;
    }

    return NULL;
}

const char* svr_vdf_section_find_value_or(SvrVdfSection* priv, const char* key, const char* def)
{
    SvrVdfKeyValue* kv = svr_vdf_section_find_kv(priv, key);

    if (kv == NULL)
    {
        return def;
    }

    return kv->value;
}

void svr_vdf_free(SvrVdfSection* root)
{
    svr_vdf_section_free(root);
    svr_free(root);
}

SvrVdfSection* svr_vdf_parse_state_get_cur_section(SvrVdfParseState* priv)
{
    assert(priv->section_stack.size > 0);
    return priv->section_stack[priv->section_stack.size - 1];
}

void svr_vdf_parse_state_push_section(SvrVdfParseState* priv, const char* name)
{
    SvrVdfSection* cur_section = svr_vdf_parse_state_get_cur_section(priv);
    SvrVdfSection* new_section = svr_vdf_section_add_section(cur_section, name);

    priv->section_stack.push(new_section);
}

void svr_vdf_parse_state_pop_section(SvrVdfParseState* priv)
{
    assert(priv->section_stack.size > 1); // Must have root still.
    priv->section_stack.size--;
}

// Add a new keyvalue to the current section in the stack.
void svr_vdf_parse_state_add_kv(SvrVdfParseState* priv, const char* key, const char* value)
{
    SvrVdfSection* cur_section = svr_vdf_parse_state_get_cur_section(priv);
    svr_vdf_section_add_kv(cur_section, key, value);
}

void svr_vdf_state_parse_line(SvrVdfParseState* parse_state, const char* line, SvrVdfLineType type)
{
    // At most, one line can have a key and a value.
    char first_part[512];
    char second_part[512];

    first_part[0] = 0;
    second_part[0] = 0;

    const char* ptr = line;
    ptr = svr_advance_until_after_whitespace(ptr); // Go past indentation.

    switch (type)
    {
        // Section starts only have a key and not a value.
        // They may or may not be in quotes.
        case SVR_VDF_LINE_SECTION_NAME:
        {
            svr_extract_string(ptr, first_part, SVR_ARRAY_SIZE(first_part));

            svr_vdf_parse_state_push_section(parse_state, first_part);
            break;
        }

        // Key values have two values.
        // Both the key and the value may or may not be in quotes.
        case SVR_VDF_LINE_KV:
        {
            ptr = svr_extract_string(ptr, first_part, SVR_ARRAY_SIZE(first_part));
            ptr = svr_advance_until_after_whitespace(ptr); // Go to second part.
            ptr = svr_extract_string(ptr, second_part, SVR_ARRAY_SIZE(second_part));

            svr_vdf_parse_state_add_kv(parse_state, first_part, second_part);
            break;
        }

        case SVR_VDF_LINE_SECTION_END:
        {
            svr_vdf_parse_state_pop_section(parse_state);
            break;
        }
    }
}

bool svr_vdf_section_is_root(SvrVdfSection* section)
{
    return section->name == NULL;
}

SvrVdfSection* svr_vdf_load(const char* path)
{
    char* file_mem = svr_read_file_as_string(path);

    if (file_mem == NULL)
    {
        return NULL;
    }

    SvrVdfSection* root = SVR_ZALLOC(SvrVdfSection);

    char line[8192];

    const char* prev_str = file_mem;

    SvrVdfParseState parse_state = {};
    parse_state.section_stack.push(root);

    while (true)
    {
        const char* next_str = svr_read_line(prev_str, line, SVR_ARRAY_SIZE(line));

        SvrVdfLineType type = svr_vdf_categorize_line(line);

        if (type != SVR_VDF_LINE_NONE)
        {
            svr_vdf_state_parse_line(&parse_state, line, type);
        }

        prev_str = next_str;

        if (*next_str == 0)
        {
            break;
        }
    }

    assert(parse_state.section_stack.size == 1);

    return root;
}
