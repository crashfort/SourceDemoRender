#include <svr/config.hpp>
#include <svr/log_format.hpp>
#include <svr/util_windows.hpp>
#include <svr/graphics.hpp>
#include <svr/graphics_preview.hpp>
#include <svr/thread_context.hpp>
#include <svr/synchro.hpp>
#include <svr/ui.hpp>
#include <svr/movie.hpp>
#include <svr/mem.hpp>
#include <svr/os.hpp>
#include <svr/reverse.hpp>
#include <svr/platform.hpp>
#include <svr/game.hpp>

#include <mutex>
#include <charconv>

#include <d3d9.h>
#include <wrl.h>

using namespace std::literals;

struct profiler
{
    void enter()
    {
        start_point = std::chrono::high_resolution_clock::now();
    }

    void exit()
    {
        hits++;

        auto end_point = std::chrono::high_resolution_clock::now();

        total += std::chrono::duration_cast<decltype(total)>(end_point - start_point);
    }

    int64_t hits = 0;
    std::chrono::high_resolution_clock::time_point start_point;
    std::chrono::milliseconds total = {};
};

/*
int main(int argc, char* argv[])
{
    using namespace svr;

    std::mutex log_mutex;

    log_set_function([](void* context, const char* text)
    {
        auto log_mutex = static_cast<std::mutex*>(context);
        std::lock_guard g(*log_mutex);
        std::printf(text);
    }, &log_mutex);

    game_external_init("test.json");

    return 0;

    auto graphics = graphics_backend_create_d3d11();

    graphics_texture_desc bb_desc = {};
    bb_desc.width = 1280;
    bb_desc.height = 720;
    bb_desc.format = graphics_texture_format_b8g8r8a8_unorm;
    bb_desc.usage = graphics_resource_usage_default;
    bb_desc.view_access = graphics_view_access_srv | graphics_view_access_rtv;
    bb_desc.caps = graphics_resource_cap_downloadable;
    bb_desc.d2d1_target = true;

    auto bb = graphics->create_texture("bb", bb_desc);
    auto bb_srv = graphics->get_texture_srv(bb);
    auto bb_rtv = graphics->get_texture_rtv(bb);

    graphics_texture_load_desc linus_desc = {};
    linus_desc.usage = graphics_resource_usage_immutable;
    linus_desc.view_access = graphics_view_access_srv;

    auto linus = graphics->create_texture_from_file("1linus.png", "1linus.png", linus_desc);
    auto linus_srv = graphics->get_texture_srv(linus);

    graphics_text_format_desc text_desc = {};
    text_desc.font_family = "Arial";
    text_desc.font_size = 512;
    text_desc.color_r = 255;
    text_desc.color_g = 0;
    text_desc.color_b = 0;
    text_desc.color_a = 255;
    text_desc.font_style = "normal";
    text_desc.font_weight = "normal";
    text_desc.font_stretch = "normal";
    text_desc.text_align = "center";
    text_desc.paragraph_align = "center";

    auto text_item = graphics->create_text_format("counter text", bb, text_desc);

    graphics_conversion_context_desc conv_desc = {};
    conv_desc.width = bb_desc.width;
    conv_desc.height = bb_desc.height;
    conv_desc.source_format = bb_desc.format;
    conv_desc.dest_format = media_pixel_format_yuv420;
    conv_desc.dest_color_space = MEDIA_COLOR_SPACE_YUV709;

    auto conv_context = graphics->create_conversion_context("conversion context", conv_desc);

    movie_ffmpeg_log_enable(false);

    movie* mov = nullptr;
    thread_context_event movie_thread;

    profiler proc_profiler;
    profiler encode_profiler;
    profiler encode_slack_profiler;
    profiler alloc_profiler;
    profiler download_profiler;

    movie_thread.run_task([&]()
    {
        mov = movie_create_ffmpeg();

        mov->set_output("video.mp4");
        mov->set_video_pixel_format(conv_desc.dest_format);
        mov->set_media_flags(media_type_flag_video);
        mov->set_video_dimensions(bb_desc.width, bb_desc.height);
        mov->set_video_x264_crf(0);
        mov->set_video_x264_intra(false);
        mov->set_video_color_space(conv_desc.dest_color_space);
        mov->set_video_encoder(media_video_encoder_libx264);
        mov->set_video_fps(144);
        mov->set_video_x264_preset("ultrafast");

        mov->open();
    });

    auto y_size = media_calc_plane_size(conv_desc.dest_format, conv_desc.width, conv_desc.height, 0);
    auto u_size = media_calc_plane_size(conv_desc.dest_format, conv_desc.width, conv_desc.height, 1);
    auto v_size = media_calc_plane_size(conv_desc.dest_format, conv_desc.width, conv_desc.height, 2);

    proc_profiler.enter();

    for (size_t i = 0; i < 5'000; i++)
    {
        float clear_col[4] = { 0, 0, 0, 1 };
        graphics->clear_rtv(bb_rtv, clear_col);

        char buf[32];
        auto end = std::to_chars(buf, buf + sizeof(buf), i);
        *end.ptr = 0;

        graphics_overlay_desc overlay_desc = {};
        overlay_desc.sampler_state = graphics_sampler_state_type_linear;
        overlay_desc.blend_state = graphics_blend_state_type_alpha_blend;
        overlay_desc.rect.x = bb_desc.width / 2 - 128;
        overlay_desc.rect.y = bb_desc.height / 2 - 128;
        overlay_desc.rect.w = 256;
        overlay_desc.rect.h = 256;

        graphics->draw_overlay(linus_srv, bb_rtv, overlay_desc);

        constexpr auto padding = 48;
        graphics->draw_text(text_item, buf, 0 + padding, 0 + padding, bb_desc.width - padding, bb_desc.height - padding);

        graphics_texture* results[3];
        graphics->convert_pixel_formats(bb_srv, conv_context, results, 3);

        alloc_profiler.enter();

        mem_buffer y;
        mem_buffer_create(&y, y_size);

        mem_buffer u;
        mem_buffer_create(&u, u_size);

        mem_buffer v;
        mem_buffer_create(&v, v_size);

        alloc_profiler.exit();

        download_profiler.enter();
        graphics->download_texture(results[0], y.data, y.size);
        graphics->download_texture(results[1], u.data, u.size);
        graphics->download_texture(results[2], v.data, v.size);
        download_profiler.exit();

        movie_thread.run_task([&mov, &encode_profiler, y, u, v]() mutable
        {
            encode_profiler.enter();

            void* mov_planes[] = {
                y.data,
                u.data,
                v.data,
            };

            mov->push_raw_video_data(mov_planes, 3);

            mem_buffer_free(&y);
            mem_buffer_free(&u);
            mem_buffer_free(&v);

            encode_profiler.exit();
        });
    }

    proc_profiler.exit();

    graphics_backend_destroy(graphics);

    movie_thread.run_task_wait([&]()
    {
        encode_slack_profiler.enter();
        mov->close();
        movie_destroy(mov);
        encode_slack_profiler.exit();
    });

    auto report_profiler = [](std::string_view name, const profiler& value)
    {
        auto avg = value.total.count() / value.hits;
        log("================ {}: acc={}ms avg={}ms ({})\n"sv, name, value.total.count(), avg, value.hits);
    };

    report_profiler("proc"sv, proc_profiler);
    report_profiler("encode"sv, encode_profiler);
    report_profiler("encode slack"sv, encode_slack_profiler);
    report_profiler("alloc"sv, alloc_profiler);
    report_profiler("download"sv, download_profiler);

    return 0;
}
*/