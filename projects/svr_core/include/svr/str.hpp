#pragma once
#include <stdint.h>
#include <string.h>

namespace svr
{
    // Simple stack reserved string builder for practical strings.
    struct str_builder
    {
        inline void append(const char* value)
        {
            if (pos >= sizeof(buf) - 1)
            {
                return;
            }

            auto length = strlen(value);
            auto dest = buf + pos;

            if (length > sizeof(buf) - pos)
            {
                length = sizeof(buf) - pos - 1;
            }

            memcpy(dest, value, length);
            dest[length] = 0;

            pos += length;
        }

        inline void reset()
        {
            pos = 0;
        }

        char buf[512 - sizeof(size_t)];
        size_t pos = 0;
    };

    inline const char* str_trim_left(const char* value)
    {
        while (*value == ' ')
        {
            value++;
        }

        return value;
    }

    inline bool str_starts_with(const char* str, const char* prefix)
    {
        while (*prefix)
        {
            if (*prefix != *str)
            {
                return false;
            }

            prefix++;
            str++;
        }

        return true;
    }

    inline bool str_ends_with(const char* str, const char* suffix)
    {
        auto str_len = strlen(str);
        auto suffix_len = strlen(suffix);

        if (suffix_len > str_len)
        {
            return false;
        }

        str += str_len - suffix_len;

        return strcmp(str, suffix) == 0;
    }
}
