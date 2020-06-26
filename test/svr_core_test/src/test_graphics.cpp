#include <catch2/catch.hpp>

#include <svr/graphics.hpp>
#include <svr/graphics_preview.hpp>
#include <svr/str.hpp>
#include <svr/os.hpp>
#include <svr/defer.hpp>
#include <svr/ui.hpp>
#include <svr/thread_context.hpp>
#include <svr/synchro.hpp>

#include <charconv>

static const auto WIDTH = 1024;
static const auto HEIGHT = 1024;

struct color
{
    float r;
    float g;
    float b;
    float a;
};

static color CLEAR_COLORS[] = {
    color {1, 0, 0, 1},
    color {0, 1, 0, 1},
    color {0, 0, 1, 1},
    color {1, 1, 0, 1},
    color {1, 0, 1, 1},
    color {0, 1, 1, 1},
};

TEST_CASE("preview")
{
    using namespace svr;

    char cur_dir[512];
    REQUIRE(os_get_current_dir(cur_dir, sizeof(cur_dir)));

    auto graphics = graphics_create_d3d11_backend(cur_dir);
    REQUIRE(graphics);

    defer {
        graphics_destroy_backend(graphics);
    };

    graphics_texture_desc tex_desc = {};
    tex_desc.width = WIDTH;
    tex_desc.height = HEIGHT;
    tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;

    auto tex = graphics->create_texture("tex", tex_desc);
    REQUIRE(tex);

    auto tex_srv = graphics->get_texture_srv(tex);
    auto tex_rtv = graphics->get_texture_rtv(tex);

    thread_context_event ui_thread;
    graphics_preview* prev = nullptr;

    synchro_barrier start_barrier;

    ui_thread.run_task([&]()
    {
        prev = graphics_preview_create_winapi(graphics, WIDTH, HEIGHT, false);
        start_barrier.open();
        ui_enter_message_loop();
        graphics_preview_destroy(prev);
        prev = nullptr;
    });

    start_barrier.wait();

    for (size_t i = 0; i < 15; i++)
    {
        auto col = CLEAR_COLORS[i % 6];

        float asd[] = { col.r, col.g, col.b, col.a };

        graphics->clear_rtv(tex_rtv, asd);

        prev->render(tex_srv);

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    ui_exit_message_loop(ui_thread.get_thread_id());
}

TEST_CASE("motion sample")
{
    using namespace svr;

    char cur_dir[512];
    REQUIRE(os_get_current_dir(cur_dir, sizeof(cur_dir)));

    auto graphics = graphics_create_d3d11_backend(cur_dir);
    REQUIRE(graphics);

    defer {
        graphics_destroy_backend(graphics);
    };

    graphics_texture_desc tex_desc = {};
    tex_desc.width = WIDTH;
    tex_desc.height = HEIGHT;
    tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_UAV;

    auto tex = graphics->create_texture("tex", tex_desc);
    REQUIRE(tex);

    auto tex_srv = graphics->get_texture_srv(tex);
    auto tex_uav = graphics->get_texture_uav(tex);

    graphics_texture_desc ov_desc = {};
    ov_desc.width = WIDTH;
    ov_desc.height = HEIGHT;
    ov_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    ov_desc.usage = GRAPHICS_USAGE_DEFAULT;
    ov_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;

    auto ov = graphics->create_texture("ov", ov_desc);
    REQUIRE(ov);

    auto ov_srv = graphics->get_texture_srv(ov);
    auto ov_rtv = graphics->get_texture_rtv(ov);

    float clear_col[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    graphics->clear_rtv(ov_rtv, clear_col);

    thread_context_event ui_thread;
    graphics_preview* prev = nullptr;

    synchro_barrier start_barrier;

    ui_thread.run_task([&]()
    {
        prev = graphics_preview_create_winapi(graphics, WIDTH, HEIGHT, false);
        start_barrier.open();
        ui_enter_message_loop();
        graphics_preview_destroy(prev);
        prev = nullptr;
    });

    start_barrier.wait();

    for (size_t i = 0; i < 255; i++)
    {
        graphics_motion_sample_desc motion_desc = {};
        motion_desc.width = WIDTH;
        motion_desc.height = HEIGHT;

        graphics->motion_sample(motion_desc, ov_srv, tex_uav, 1.0f / 255.0f);

        prev->render(tex_srv);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ui_exit_message_loop(ui_thread.get_thread_id());
}

TEST_CASE("text")
{
    using namespace svr;

    char cur_dir[512];
    REQUIRE(os_get_current_dir(cur_dir, sizeof(cur_dir)));

    auto graphics = graphics_create_d3d11_backend(cur_dir);
    REQUIRE(graphics);

    defer {
        graphics_destroy_backend(graphics);
    };

    graphics_texture_desc tex_desc = {};
    tex_desc.width = WIDTH;
    tex_desc.height = HEIGHT;
    tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;
    tex_desc.text_target = true;

    auto tex = graphics->create_texture("tex", tex_desc);
    REQUIRE(tex);

    auto tex_srv = graphics->get_texture_srv(tex);
    auto tex_rtv = graphics->get_texture_rtv(tex);

    graphics_text_format_desc text_desc = {};
    text_desc.font_family = "Arial";
    text_desc.font_size = 72;
    text_desc.color_r = 255;
    text_desc.color_g = 255;
    text_desc.color_b = 255;
    text_desc.color_a = 255;

    auto text_format = graphics->create_text_format("text_format", tex, text_desc);
    REQUIRE(text_format);

    thread_context_event ui_thread;
    graphics_preview* prev = nullptr;

    synchro_barrier start_barrier;

    ui_thread.run_task([&]()
    {
        prev = graphics_preview_create_winapi(graphics, WIDTH, HEIGHT, false);
        start_barrier.open();
        ui_enter_message_loop();
        graphics_preview_destroy(prev);
        prev = nullptr;
    });

    start_barrier.wait();

    for (size_t i = 0; i < 15; i++)
    {
        float clear_col[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        graphics->clear_rtv(tex_rtv, clear_col);

        char buf[128];
        auto res = std::to_chars(buf, buf + sizeof(buf), i);
        *res.ptr = 0;

        graphics->draw_text(text_format, buf, 0, 0, WIDTH, HEIGHT);

        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        prev->render(tex_srv);
    }

    ui_exit_message_loop(ui_thread.get_thread_id());
}

TEST_CASE("overlay")
{
    using namespace svr;

    char cur_dir[512];
    REQUIRE(os_get_current_dir(cur_dir, sizeof(cur_dir)));

    auto graphics = graphics_create_d3d11_backend(cur_dir);
    REQUIRE(graphics);

    defer {
        graphics_destroy_backend(graphics);
    };

    graphics_texture_desc tex_desc = {};
    tex_desc.width = WIDTH;
    tex_desc.height = HEIGHT;
    tex_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;

    auto tex = graphics->create_texture("tex", tex_desc);
    REQUIRE(tex);

    auto tex_srv = graphics->get_texture_srv(tex);
    auto tex_rtv = graphics->get_texture_rtv(tex);

    graphics_texture_desc ov_desc = {};
    ov_desc.width = 128;
    ov_desc.height = 128;
    ov_desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    ov_desc.usage = GRAPHICS_USAGE_DEFAULT;
    ov_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_RTV;

    auto ov = graphics->create_texture("ov", ov_desc);
    REQUIRE(ov);

    auto ov_srv = graphics->get_texture_srv(ov);
    auto ov_rtv = graphics->get_texture_rtv(ov);

    float ov_clear_col[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    graphics->clear_rtv(ov_rtv, ov_clear_col);

    thread_context_event ui_thread;
    graphics_preview* prev = nullptr;

    synchro_barrier start_barrier;

    ui_thread.run_task([&]()
    {
        prev = graphics_preview_create_winapi(graphics, WIDTH, HEIGHT, false);
        start_barrier.open();
        ui_enter_message_loop();
        graphics_preview_destroy(prev);
        prev = nullptr;
    });

    start_barrier.wait();

    for (size_t i = 0; i < 150; i++)
    {
        float tex_clear_col[] = { 0.0f, 0.0f, 1.0f, 1.0f };
        graphics->clear_rtv(tex_rtv, tex_clear_col);

        graphics_overlay_desc overlay_desc = {};
        overlay_desc.rect.x = (cos(i / 0.04f) * 512.0f) + (WIDTH / 2) - 64;
        overlay_desc.rect.y = (sin(i / 0.04f) * 512.0f) + (WIDTH / 2) - 64;
        overlay_desc.rect.w = 128;
        overlay_desc.rect.h = 128;
        overlay_desc.sampler_state = GRAPHICS_SAMPLER_POINT;
        overlay_desc.blend_state = GRAPHICS_BLEND_OPAQUE;

        graphics->draw_overlay(ov_srv, tex_rtv, overlay_desc);

        prev->render(tex_srv);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    ui_exit_message_loop(ui_thread.get_thread_id());
}
