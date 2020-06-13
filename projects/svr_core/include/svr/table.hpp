#pragma once
#include <string.h>

// Bidirectional string to structure mappings.

namespace svr
{
    template <class T>
    struct table_pair
    {
        const char* first;
        T second;
    };

    // Aggregate guide.

    template <class T>
    table_pair(const char*, T)->table_pair<T>;

    template<class T, size_t Size>
    struct table
    {
        static const size_t SIZE = Size;
        T elements[Size];
    };

    // Aggregate guide.

    template <class First, class... Rest>
    table(First, Rest...)->table<First, 1 + sizeof...(Rest)>;

    template <class TableT, class T>
    inline T table_map_key_or(TableT tab, const char* key, T def)
    {
        for (size_t i = 0; i < TableT::SIZE; i++)
        {
            const auto& e = tab.elements[i];

            if (strcmp(key, e.first) == 0)
            {
                return e.second;
            }
        }

        return def;
    }

    template <class TableT, class T>
    inline const char* table_map_value_or(TableT tab, T value, const char* def)
    {
        for (size_t i = 0; i < TableT::SIZE; i++)
        {
            const auto& e = tab.elements[i];

            if (e.second == value)
            {
                return e.first;
            }
        }

        return def;
    }
}
