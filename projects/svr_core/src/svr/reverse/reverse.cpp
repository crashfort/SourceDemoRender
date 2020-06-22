#include <svr/reverse.hpp>
#include <svr/log_format.hpp>

#include <vector>
#include <charconv>

struct reverse_pattern
{
    // Negative value means unknown byte.
    int16_t bytes[256];
    size_t used = 0;
};

static bool pattern_bytes_from_string(const char* input, reverse_pattern& out)
{
    auto expect_space = false;
    auto it = input;

    while (*it)
    {
        if (*it == ' ')
        {
            ++it;
            expect_space = false;
        }

        else if (expect_space)
        {
            auto distance = it - input;

            svr::log("reverse: Parse error: Expected whitespace near index {}\n", distance);
            return false;
        }

        if ((*it >= '0' && *it <= '9') || (*it >= 'A' && *it <= 'F'))
        {
            auto res = std::from_chars(it, it + 2, out.bytes[out.used], 16);

            if (res.ec != std::errc())
            {
                auto distance = it - input;

                svr::log("reverse: Parse error: Invalid hex conversion near index {}\n", distance);
                return false;
            }

            out.used++;

            it += 2;

            expect_space = true;
        }

        else if (*it == '?')
        {
            out.bytes[out.used] = -1;
            out.used++;

            it += 2;

            expect_space = true;
        }
    }

    if (out.used == 0)
    {
        svr::log("reverse: No bytes in pattern '{}'\n", input);
        return false;
    }

    return true;
}

static bool compare_data(const uint8_t* data, const reverse_pattern& pattern)
{
    auto index = 0;

    auto bytes = pattern.bytes;

    for (size_t i = 0; i < pattern.used; i++)
    {
        auto byte = *bytes;

        if (byte > -1 && *data != byte)
        {
            return false;
        }

        ++data;
        ++bytes;
        ++index;
    }

    return index == pattern.used;
}

static void* find_pattern(void* start, size_t search_length, const reverse_pattern& pattern)
{
    auto length = pattern.used;

    for (size_t i = 0; i <= search_length - length; ++i)
    {
        auto addr = static_cast<uint8_t*>(start) + i;

        if (compare_data(addr, pattern))
        {
            return addr;
        }
    }

    return nullptr;
}

namespace svr
{
    void* reverse_find_pattern(void* base, size_t size, const char* pattern)
    {
        reverse_pattern bytes;

        if (!pattern_bytes_from_string(pattern, bytes))
        {
            log("Could not parse pattern '{}'\n", pattern);
            return nullptr;
        }

        return find_pattern(base, size, bytes);
    }

    void* reverse_add_offset(void* address, ptrdiff_t offset)
    {
        auto mod = static_cast<uint8_t*>(address);
        mod += offset;

        return mod;
    }

    void* reverse_follow_rel_jump(void* address)
    {
        auto mod = static_cast<uint8_t*>(address);

        if (*mod != 0xE8)
        {
            log("reverse: Could not follow relative jump. Got {:x} expected 0xE8 at {}\n", *mod, address);
            return nullptr;
        }

        // Skip the E8 byte.
        mod += sizeof(uint8_t);

        auto offset = *reinterpret_cast<ptrdiff_t*>(mod);

        // E8 jumps count relative distance from the next instruction.
        mod += sizeof(uintptr_t);

        // Do the jump, address will now be the target function.
        mod += offset;

        return mod;
    }
}
