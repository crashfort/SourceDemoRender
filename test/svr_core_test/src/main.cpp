#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <svr/log.hpp>

#include <stdio.h>

int main(int argc, char* argv[])
{
    using namespace svr;

    log_set_function([](void* context, const char* text)
    {
        fwrite(text, 1, strlen(text), stdout);
    }, nullptr);

    auto ret = Catch::Session().run(argc, argv);

    fgetc(stdin);

    return ret;
}
