#include <svr/log.hpp>
#include <svr/game.hpp>

#include <svr/graphics.hpp>
#include <svr/defer.hpp>
#include <svr/log_format.hpp>
#include <svr/mem.hpp>
#include <svr/os.hpp>
#include <svr/str.hpp>
#include <svr/tokenize.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include <stb_image_write.h>

// #define CATCH_CONFIG_RUNNER
// #include <catch2/catch.hpp>

#include <stdio.h>
#include <algorithm>

static void test_func()
{
    using namespace svr;

    auto graphics = graphics_backend_create_d3d11("C:/FAST GitHub/source-video-render/bin/");

    graphics_texture_desc source_tex_desc = {};
    source_tex_desc.width = 1024;
    source_tex_desc.height = 1024;
    source_tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    source_tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    source_tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;

    auto source_tex = graphics->create_texture("source", source_tex_desc);
    auto source_tex_srv = graphics->get_texture_srv(source_tex);
    auto source_tex_rtv = graphics->get_texture_rtv(source_tex);

    graphics_texture_desc dest_tex_desc = {};
    dest_tex_desc.width = 1024;
    dest_tex_desc.height = 1024;
    dest_tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    dest_tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    dest_tex_desc.view_access = GRAPHICS_VIEW_RTV | GRAPHICS_VIEW_UAV;
    dest_tex_desc.caps = GRAPHICS_CAP_DOWNLOADABLE;

    auto dest_tex = graphics->create_texture("dest", dest_tex_desc);
    auto dest_tex_rtv = graphics->get_texture_rtv(dest_tex);
    auto dest_tex_uav = graphics->get_texture_uav(dest_tex);

    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    graphics->clear_rtv(source_tex_rtv, white);
    graphics->clear_rtv(dest_tex_rtv, black);

    graphics_motion_sample_desc sample_desc;
    sample_desc.width = 1024;
    sample_desc.height = 1024;

    mem_buffer buf;
    mem_create_buffer(buf, graphics->get_texture_size(dest_tex));

    for (size_t i = 0; i < 61; i++)
    {
        graphics->motion_sample(sample_desc, source_tex_srv, dest_tex_uav, 1.0f / 60.0f);

        graphics->download_texture(dest_tex, buf.data, buf.size);

        auto name = fmt::format("{}.png", i);

        stbi_write_png(name.data(), 1024, 1024, 4, buf.data, 1024 * 4);
    }
}

static void test_func_2()
{
    using namespace svr;

    char buf[1024];
    os_get_module_name(os_get_proc_handle_self(), os_get_module("svr_core.dll"), buf, sizeof(buf));

    int a = 5;
}

static void test_func_3()
{
    using namespace svr;

    auto list = os_list_files("C:/FAST GitHub/source-video-render/bin/data/profiles");

    if (list == nullptr)
    {
        return;
    }

    defer {
        os_destroy_file_list(list);
    };

    auto size = os_file_list_size(list);

    for (size_t i = 0; i < size; i++)
    {
        log("'{}' '{}' '{}'\n", os_file_list_path(list, i), os_file_list_name(list, i), os_file_list_ext(list, i));
    }
}

static void test_func_4()
{
    using namespace svr;

    auto a1 = tokenize("       surf_0                 a.mp4 whatever something here i guess this will be a lot of tokens lol              mp4");
    auto a2 = tokenize("startmovie surf_0 a.mp4");
    auto a3 = tokenize("startmovie a.mp4");
    auto a4 = tokenize("");
    auto a5 = tokenize("startmovie");
    auto a6 = tokenize("          ");

    int a = 5;
}

static bool launch_momentum()
{
    return svr::game_launch_inject("C:/Program Files (x86)/Steam/steamapps/common/Momentum Mod/hl2.exe",
                                   "C:/Program Files (x86)/Steam/steamapps/common/Momentum Mod/momentum",
                                   "mom-win",
                                   "-window -w 1920 -h 1080 -nojoy -dev -game momentum",
                                   "C:/FAST GitHub/source-video-render/bin/");
}

static bool launch_csgo()
{
    return svr::game_launch_inject("B:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/csgo.exe",
                                   "B:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/csgo",
                                   "csgo-win",
                                   "-window -w 1920 -h 1080 -nojoy -dev -game csgo",
                                   "C:/FAST GitHub/source-video-render/bin/");
}

static void agfdagfdafgsd(const char* gjfkda)
{

}

int main(int argc, char* argv[])
{
    using namespace svr;

    log_set_function([](void* context, const char* text)
    {
        printf(text);
    }, nullptr);

    auto res = launch_csgo();

    // Return 0 on success.
    return !res;
}
