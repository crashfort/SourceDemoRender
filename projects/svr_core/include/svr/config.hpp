#pragma once
#include <svr/api.hpp>

#include <stdint.h>

// Provides read only access to json.

namespace svr
{
    struct config;

    // View of a node within a config.
    // Is only valid as long as the parent config is alive.
    struct config_node;

    // Attempts to open a file and parse the contents as json.
    SVR_API config* config_open_json(const char* path);

    // Attempts to parse a memory buffer as json.
    SVR_API config* config_parse_json(const char* data, size_t size);

    // Destroys a config.
    // All views become invalidated and should no longer be used.
    SVR_API void config_destroy(config* ptr);

    // Returns the root element in a document.
    SVR_API config_node* config_root(config* ptr);

    SVR_API config_node* config_find(config_node* node, const char* name);
    SVR_API bool config_get_bool(config_node* node, bool* out);
    SVR_API bool config_get_int64(config_node* node, int64_t* out);
    SVR_API bool config_get_float(config_node* node, float* out);
    SVR_API const char* config_get_string(config_node* node, size_t* length = nullptr);
    SVR_API size_t config_get_array_size(config_node* node);
    SVR_API config_node* config_get_array_element(config_node* node, size_t index);

    inline bool config_view_bool_or(config_node* n, bool def)
    {
        config_get_bool(n, &def);
        return def;
    }

    inline int64_t config_view_int64_or(config_node* n, int64_t def)
    {
        config_get_int64(n, &def);
        return def;
    }

    inline float config_view_float_or(config_node* n, float def)
    {
        config_get_float(n, &def);
        return def;
    }

    inline const char* config_view_string_or(config_node* n, const char* def, size_t* len = nullptr)
    {
        auto ptr = config_get_string(n, len);

        if (ptr)
        {
            return ptr;
        }

        return def;
    }

    class config_node_iterator
    {
    private:
        class iterator
        {
        public:
            iterator(config_node* view, size_t i)
            {
                node_view = view;
                index = i;
            }

            iterator operator++()
            {
                ++index;
                return *this;
            }

            iterator operator--()
            {
                --index;
                return *this;
            }

            bool operator!=(const iterator& other) const
            {
                return index != other.index;
            }

            config_node* operator*()
            {
                return config_get_array_element(node_view, index);
            }

        private:
            size_t index;
            config_node* node_view;
        };

    public:
        config_node_iterator(config_node* view)
        {
            node_view = view;
        }

        auto begin()
        {
            return iterator(node_view, 0);
        }

        auto end()
        {
            return iterator(node_view, config_get_array_size(node_view));
        }

    private:
        config_node* node_view;
    };
}
