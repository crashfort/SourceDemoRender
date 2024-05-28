#include "game_priv.h"

// Memory scanning.

// How many bytes there can be in a pattern scan.
const s32 GAME_MAX_SCAN_BYTES = 256;

struct GameScanPattern
{
    // A value of -1 means unknown byte.
    s16 bytes[GAME_MAX_SCAN_BYTES];
    s16 used;
};

bool game_is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

void game_pattern_bytes_from_string(const char* input, GameScanPattern* out)
{
    const char* ptr = input;

    out->used = 0;

    for (; *ptr != 0; ptr++)
    {
        assert(out->used < GAME_MAX_SCAN_BYTES);

        if (game_is_hex_char(*ptr))
        {
            assert(game_is_hex_char(*(ptr + 1))); // Next must be the next 4 bits.

            out->bytes[out->used] = strtol(ptr, NULL, 16);
            out->used++;
            ptr++;
        }

        else if (*ptr == '?')
        {
            assert(*(ptr + 1) == '?'); // Next must be question mark.

            out->bytes[out->used] = -1;
            out->used++;
            ptr++;
        }
    }

    assert(out->used > 0); // Must have written something.
}

bool game_compare_data(u8* data, GameScanPattern* pattern)
{
    s32 index = 0;
    s16* bytes = pattern->bytes;

    for (s32 i = 0; i < pattern->used; i++)
    {
        s16 byte = *bytes;

        if (byte > -1 && *data != byte)
        {
            return false;
        }

        data++;
        bytes++;
        index++;
    }

    return index == pattern->used;
}

void* game_find_pattern(void* start, s32 search_length, GameScanPattern* pattern)
{
    s16 length = pattern->used;

    for (s32 i = 0; i <= search_length - length; i++)
    {
        u8* addr = (u8*)start + i;

        if (game_compare_data(addr, pattern))
        {
            return addr;
        }
    }

    return NULL;
}

void* game_scan_pattern(const char* dll, const char* pattern, void* from)
{
    MODULEINFO info;

    if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(dll), &info, sizeof(MODULEINFO)))
    {
        // Module is not loaded. Not an error because we allow fallthrough scanning of multiple patterns.
        return NULL;
    }

    GameScanPattern pattern_bytes = {};
    game_pattern_bytes_from_string(pattern, &pattern_bytes);

    if (from == NULL)
    {
        from = info.lpBaseOfDll;
    }

    else
    {
        // Start address must be in range of the module.
        assert(((u8*)from >= info.lpBaseOfDll) && (u8*)from < ((u8*)info.lpBaseOfDll + info.SizeOfImage));
    }

    void* ret = game_find_pattern(from, info.SizeOfImage, &pattern_bytes);
    return ret;
}
