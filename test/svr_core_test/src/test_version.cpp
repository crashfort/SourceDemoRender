#include <catch2/catch.hpp>

#include <svr/version.hpp>

TEST_CASE("version")
{
    using namespace svr;

    REQUIRE(version_greater_than(version_pair { 2, 0 }, version_pair { 1, 0 }));
    REQUIRE_FALSE(version_greater_than(version_pair { 1, 0 }, version_pair { 2, 0 }));

    REQUIRE(version_greater_than(version_pair { 1, 100 }, version_pair { 1, 0 }));
    REQUIRE_FALSE(version_greater_than(version_pair { 1, 0 }, version_pair { 1, 100 }));

    REQUIRE(version_greater_than(version_pair { 2, 0 }, version_pair { 1, 100 }));
    REQUIRE_FALSE(version_greater_than(version_pair { 1, 100 }, version_pair { 2, 0 }));
    REQUIRE_FALSE(version_greater_than(version_pair { 0, 3 }, version_pair { 1, 0 }));
}
