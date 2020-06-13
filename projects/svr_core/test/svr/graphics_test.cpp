#include <svr/core_graphics.hpp>
#include <svr/movie.hpp>
#include <svr/log_format.hpp>
#include <svr/mem.hpp>
#include <svr/os.hpp>
#include <svr/core_graphics_preview.hpp>
#include <svr/thread.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

#include <cstdio>
#include <mutex>
#include <thread>
#include <chrono>

namespace
{
    svr::core_graphics_texture* create_nature_texture(svr::core_graphics_backend* graphics)
    {
        svr::core_graphics_texture_load_desc desc = {};
        desc.usage = svr::core_graphics_resource_usage_default;
        desc.view_access = svr::core_graphics_view_access_srv | svr::core_graphics_view_access_uav | svr::core_graphics_view_access_rtv;
        desc.caps = svr::core_graphics_resource_cap_downloadable;

        return graphics->create_texture_from_file("fnv.png", "fnv.png", desc);
    }

    svr::core_graphics_texture* create_linus_texture(svr::core_graphics_backend* graphics)
    {
        svr::core_graphics_texture_load_desc desc = {};
        desc.usage = svr::core_graphics_resource_usage_immutable;
        desc.view_access = svr::core_graphics_view_access_srv;

        return graphics->create_texture_from_file("linus.png", "linus.png", desc);
    }
}

int main(int argc, char* argv[])
{
    std::mutex log_mutex;

    svr::log_set_function([](void* context, const char* text)
    {
        auto log_mutex = static_cast<std::mutex*>(context);
        std::lock_guard g(*log_mutex);
        std::printf(text);
    }, &log_mutex);

    auto graphics = svr::core_graphics_backend_create_d3d11();
    auto movie = svr::movie_create_ffmpeg();

    auto nature_tex = create_nature_texture(graphics);
    auto nature_tex_srv = graphics->get_texture_srv(nature_tex);
    auto nature_tex_rtv = graphics->get_texture_rtv(nature_tex);
    auto nature_tex_uav = graphics->get_texture_uav(nature_tex);

    auto linus_tex = create_linus_texture(graphics);
    auto linus_tex_srv = graphics->get_texture_srv(linus_tex);

    auto nature_tex_d = graphics->get_texture_dimensions(nature_tex);
    auto linus_tex_d = graphics->get_texture_dimensions(linus_tex);

    movie->set_output("video.mp4");
    movie->set_media_flags(svr::media_type_flag_video);
    movie->set_dimensions(nature_tex_d.width, nature_tex_d.height);
    movie->set_framerate(60);
    movie->set_video_encoder(svr::media_video_encoder_libx264rgb);
    movie->set_pixel_format(svr::media_pixel_format_bgr0);
    movie->set_color_space(svr::media_color_space_rgb);

    auto res = movie->open();

    {
        svr::core_graphics_conversion_context_desc desc = {};
        desc.width = nature_tex_d.width;
        desc.height = nature_tex_d.height;
        desc.source_format = svr::core_graphics_texture_format_b8g8r8a8_unorm;
        desc.dest_format = svr::media_pixel_format_bgr0;
        desc.dest_color_space = svr::media_color_space_rgb;

        auto conversion_context = graphics->create_conversion_context("conversion context", desc);

        svr::mem_buffer_wrapper fnv;
        fnv.create(graphics->get_texture_size(nature_tex));

        graphics->download_texture(nature_tex, fnv.data(), fnv.size());

        // stbi_write_png("nature.png", 1920, 1080, 4, fnv.data(), 1920 * 4);

        if (desc.dest_format == svr::media_pixel_format_bgr0)
        {
            svr::mem_buffer_wrapper bgr0;
            bgr0.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 0)));

            for (size_t i = 0; i < 60; i++)
            {
                svr::core_graphics_texture* targets[1];
                graphics->convert_pixel_formats(nature_tex_srv, conversion_context, targets, 1);

                graphics->download_texture(targets[0], bgr0.data(), bgr0.size());

                void* video_planes[] = {
                    bgr0.data(),
                };

                movie->push_raw_video_data(video_planes, 1);
            }
        }

        if (desc.dest_format == svr::media_pixel_format_nv12)
        {
            svr::mem_buffer_wrapper y;
            y.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 0)));

            svr::mem_buffer_wrapper uv;
            uv.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 1)));

            svr::core_graphics_texture* targets[2];
            graphics->convert_pixel_formats(nature_tex_srv, conversion_context, targets, 2);

            graphics->download_texture(targets[0], y.data(), y.size());
            graphics->download_texture(targets[1], uv.data(), uv.size());

            for (size_t i = 0; i < 60; i++)
            {
                void* planes[] = {
                    y.data(),
                    uv.data(),
                };

                movie->push_raw_video_data(planes, 2);
            }
        }

        if (desc.dest_format == svr::media_pixel_format_yuv420 ||
            desc.dest_format == svr::media_pixel_format_yuv444)
        {
            svr::mem_buffer_wrapper y;
            y.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 0)));

            svr::mem_buffer_wrapper u;
            u.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 1)));

            svr::mem_buffer_wrapper v;
            v.create(graphics->get_texture_size(graphics->get_conversion_texture(conversion_context, 2)));

            for (size_t i = 0; i < 60; i++)
            {
                svr::core_graphics_texture* targets[3];
                graphics->convert_pixel_formats(nature_tex_srv, conversion_context, targets, 3);

                graphics->download_texture(targets[0], y.data(), y.size());
                graphics->download_texture(targets[1], u.data(), u.size());
                graphics->download_texture(targets[2], v.data(), v.size());

                void* planes[] = {
                    y.data(),
                    u.data(),
                    v.data(),
                };

                movie->push_raw_video_data(planes, 3);
            }
        }
    }

    svr::movie_destroy(movie);
    svr::core_graphics_backend_destroy(graphics);

    return 0;
}
