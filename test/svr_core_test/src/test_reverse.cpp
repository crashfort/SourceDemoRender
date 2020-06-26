#include <catch2/catch.hpp>

#include <svr/reverse.hpp>

#include <stdint.h>

TEST_CASE("pattern match")
{
    using namespace svr;

    uint8_t bytes[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    SECTION("known")
    {
        auto p = reverse_find_pattern(bytes, sizeof(bytes), "04 05 06");
        REQUIRE(p == bytes + 3);
    }

    SECTION("unknown")
    {
        auto p = reverse_find_pattern(bytes, sizeof(bytes), "?? ?? ?? 04 05 06");
        REQUIRE(p == bytes);
    }
}
