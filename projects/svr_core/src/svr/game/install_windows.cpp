#include <svr/os.hpp>
#include <svr/str.hpp>
#include <svr/log_format.hpp>

bool verify_installation(const char* resource_path)
{
    using namespace svr;

    const char* required_files[] = {
        "svr_game.dll",
        "ffmpeg.exe",
        "data/game-config.json",
        "data/profiles/default.json",
        "data/movies",
    };

    for (auto file : required_files)
    {
        str_builder builder;
        builder.append(resource_path);
        builder.append(file);

        if (!os_does_file_exist(builder.buf))
        {
            log("Missing required file or directory '{}'\n", builder.buf);
            return false;
        }
    }

    return true;
}
