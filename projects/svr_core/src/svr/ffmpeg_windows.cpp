#include <svr/str.hpp>

svr::str_builder build_ffmpeg_path(const char* resource_path)
{
    svr::str_builder exe;
    exe.append(resource_path);
    exe.append("ffmpeg.exe");

    return exe;
}
