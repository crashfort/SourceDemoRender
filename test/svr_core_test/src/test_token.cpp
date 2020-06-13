#include <catch2/catch.hpp>

#include <svr/tokenize.hpp>

TEST_CASE("tokenize")
{
    using namespace svr;

    auto a1 = tokenize("       startmovie                 a.mp4               profile           ");
    REQUIRE(a1.size() == 3);

    auto a2 = tokenize("startmovie a.mp4 profile");
    REQUIRE(a2.size() == 3);

    auto a3 = tokenize("startmovie a.mp4");
    REQUIRE(a3.size() == 2);

    auto a4 = tokenize("");
    REQUIRE(a4.size() == 0);

    auto a5 = tokenize("startmovie");
    REQUIRE(a5.size() == 1);

    auto a6 = tokenize("          ");
    REQUIRE(a6.size() == 0);

    auto a7 = tokenize("startmovie ");
    REQUIRE(a7.size() == 1);
}
