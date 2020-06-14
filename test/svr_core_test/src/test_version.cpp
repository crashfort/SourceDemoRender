#include <catch2/catch.hpp>

#include <svr/version.hpp>

TEST_CASE("version")
{
    using namespace svr;

    auto v1 = version_pair { 1, 0 };
    auto v2 = version_pair { 2, 0 };
    REQUIRE(version_greater_than(v2, v1));

    auto v3 = version_pair { 1, 100 };
    REQUIRE(version_greater_than(v3, v1));
    REQUIRE(version_greater_than(v2, v3));
}
