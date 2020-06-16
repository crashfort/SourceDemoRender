#include "game_system.hpp"

#include <svr/graphics.hpp>
#include <svr/movie.hpp>
#include <svr/thread_context.hpp>
#include <svr/graphics_preview.hpp>
#include <svr/format.hpp>
#include <svr/mem.hpp>
#include <svr/defer.hpp>
#include <svr/log_format.hpp>
#include <svr/config.hpp>
#include <svr/str.hpp>
#include <svr/ui.hpp>
#include <svr/swap.hpp>
#include <svr/vec.hpp>

#include <charconv>

// Common implementation for all architectures.

struct game_system
{
    const char* resource_path;

    bool movie_runing = false;
    uint32_t width;
    uint32_t height;

    svr::movie* movie = nullptr;
    svr::graphics_backend* graphics;
    svr::graphics_texture* game_content = nullptr;
    svr::graphics_texture* work_texture = nullptr;
    svr::graphics_conversion_context* px_converter = nullptr;
    svr::graphics_text_format* velocity_text_format = nullptr;
    svr::thread_context_event encode_thread;

    svr::graphics_preview* ui_preview = nullptr;
    svr::thread_context_event ui_thread;

    uint64_t frame_num;

    bool use_preview_window = false;
    bool use_motion_blur = false;

    bool support_velocity_overlay = false;
    bool use_velocity_overlay = false;

    // How many frames per second the game should use.
    uint32_t game_rate;

    float sample_remainder;
    float sample_remainder_step;
    float sample_exposure;

    // Buffer used in the encoding thread.
    // This buffer contains the data that is sent for encoding.
    // Multiple planes are combined into a single buffer.
    svr::mem_buffer movie_buf;
    size_t movie_plane_sizes[3];
    size_t movie_plane_count;

    int velocity_overlay_padding;
    svr::vec3 velocity;
};

static svr::graphics_texture* create_work_texture(game_system* sys, uint32_t width, uint32_t height)
{
    using namespace svr;

    // Must use 32 bits per channel so we get the required accuracy.

    graphics_texture_desc tex_desc = {};
    tex_desc.width = width;
    tex_desc.height = height;
    tex_desc.format = GRAPHICS_FORMAT_R32G32B32A32_FLOAT;
    tex_desc.usage = GRAPHICS_USAGE_DEFAULT;
    tex_desc.view_access = GRAPHICS_VIEW_SRV | GRAPHICS_VIEW_UAV | GRAPHICS_VIEW_RTV;
    tex_desc.caps = GRAPHICS_CAP_DOWNLOADABLE;
    tex_desc.text_target = true;

    return sys->graphics->create_texture("sys work", tex_desc);
}

static svr::graphics_conversion_context* create_pixel_converter(game_system* sys, uint32_t width, uint32_t height, svr::media_pixel_format format, svr::media_color_space color_space)
{
    using namespace svr;

    graphics_conversion_context_desc conv_desc = {};
    conv_desc.width = width;
    conv_desc.height = height;
    conv_desc.source_format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    conv_desc.dest_format = format;
    conv_desc.dest_color_space = color_space;

    return sys->graphics->create_conversion_context("sys px conv", conv_desc);
}

static bool load_movie_settings(game_system* sys, svr::config_node* n, const char* name)
{
    using namespace svr;

    sys->game_rate = config_view_int64_or(config_find(n, "video-fps"), 60);

    auto px_format = media_pixel_format_from_string(config_view_string_or(config_find(n, "video-pixel-format"), "bgr0"));
    auto color_space = media_color_space_from_string(config_view_string_or(config_find(n, "video-color-space"), "rgb"));

    sys->movie->enable_log(false);
    sys->movie->set_threads(config_view_int64_or(config_find(n, "encoding-threads"), 0));
    sys->movie->set_video_fps(sys->game_rate);
    sys->movie->set_video_encoder(config_view_string_or(config_find(n, "video-encoder"), "libx264rgb"));
    sys->movie->set_video_pixel_format(px_format);
    sys->movie->set_video_color_space(color_space);
    sys->movie->set_video_x264_crf(config_view_int64_or(config_find(n, "video-x264-crf"), 0));
    sys->movie->set_video_x264_preset(config_view_string_or(config_find(n, "video-x264-preset"), "ultrafast"));
    sys->movie->set_video_x264_intra(config_view_bool_or(config_find(n, "video-x264-intra"), false));

    str_builder builder;
    builder.append(sys->resource_path);
    builder.append("data/movies/");
    builder.append(name);

    sys->movie->set_output(builder.buf);
    sys->movie->set_video_dimensions(sys->width, sys->height);

    if (!sys->movie->open_movie())
    {
        log("Could not open movie\n");
        return false;
    }

    graphics_texture* work_tex = nullptr;
    graphics_conversion_context* px_conv = nullptr;

    defer {
        if (work_tex) sys->graphics->destroy_texture(work_tex);
        if (px_conv) sys->graphics->destroy_conversion_context(px_conv);
    };

    work_tex = create_work_texture(sys, sys->width, sys->height);
    if (work_tex == nullptr) return false;

    px_conv = create_pixel_converter(sys, sys->width, sys->height, px_format, color_space);
    if (px_conv == nullptr) return false;

    swap_ptr(sys->work_texture, work_tex);
    swap_ptr(sys->px_converter, px_conv);

    // Create a buffer that all combined video planes will be downloaded into.
    // This single buffer is then streamed to an ffmpeg process.

    size_t total_size = 0;

    sys->movie_plane_count = sys->graphics->get_conversion_texture_count(sys->px_converter);
    sys->graphics->get_conversion_sizes(sys->px_converter, sys->movie_plane_sizes, 3);

    for (size_t i = 0; i < sys->movie_plane_count; i++)
    {
        total_size += sys->movie_plane_sizes[i];
    }

    mem_create_buffer(sys->movie_buf, total_size);
    return true;
}

static bool load_motion_blur_settings(game_system* sys, svr::config_node* n)
{
    using namespace svr;

    sys->use_motion_blur = config_view_bool_or(config_find(n, "enabled"), false);

    if (!sys->use_motion_blur)
    {
        log("Not using motion blur\n");
        return true;
    }

    log("Using motion blur\n");

    auto sample_mult = config_view_int64_or(config_find(n, "fps-mult"), 32);
    sys->sample_exposure = config_view_float_or(config_find(n, "frame-exposure"), 0.5f);
    sys->sample_remainder = 0;

    // Movie is loaded beforehand, so those values are set.

    auto sps = sys->game_rate * sample_mult;
    sys->sample_remainder_step = (1.0f / sps) / (1.0f / sys->game_rate);

    sys->game_rate = sps;

    svr::log("Using {} samples per second\n", sps);

    return true;
}

static bool load_preview_window_settings(game_system* sys, svr::config_node* n)
{
    using namespace svr;

    sys->use_preview_window = config_view_bool_or(config_find(n, "enabled"), false);

    if (!sys->use_preview_window)
    {
        log("Not using preview window\n");
        return true;
    }

    log("Using preview window\n");

    return true;
}

static bool load_velocity_overlay_settings(game_system* sys, svr::config_node* n)
{
    using namespace svr;

    sys->use_velocity_overlay = config_view_bool_or(config_find(n, "enabled"), false);

    if (!sys->use_velocity_overlay)
    {
        log("Not using velocity overlay\n");
        return true;
    }

    log("Using velocity overlay\n");

    graphics_text_format_desc desc = {};
    desc.font_family = config_view_string_or(config_find(n, "font-family"), "Arial");
    desc.font_size = config_view_int64_or(config_find(n, "font-size"), 48);
    desc.color_r = config_view_int64_or(config_find(n, "color-r"), 255);
    desc.color_g = config_view_int64_or(config_find(n, "color-g"), 255);
    desc.color_b = config_view_int64_or(config_find(n, "color-b"), 255);
    desc.color_a = config_view_int64_or(config_find(n, "color-a"), 255);
    desc.font_style = config_view_string_or(config_find(n, "font-style"), "normal");
    desc.font_weight = config_view_string_or(config_find(n, "font-weight"), "normal");
    desc.font_stretch = config_view_string_or(config_find(n, "font-stretch"), "normal");
    desc.text_align = config_view_string_or(config_find(n, "text-align"), "center");
    desc.paragraph_align = config_view_string_or(config_find(n, "paragraph-align"), "center");

    sys->velocity_overlay_padding = config_view_int64_or(config_find(n, "padding"), 0);

    sys->velocity_text_format = sys->graphics->create_text_format("sys veloc text", sys->work_texture, desc);
    if (sys->velocity_text_format == nullptr) return false;

    return true;
}

static svr::config* load_profile(const char* resource_path, const char* profile)
{
    using namespace svr;

    // Hope for the best that the default profile exists.
    if (profile == nullptr)
    {
        profile = "default";
    }

    str_builder builder;
    builder.append(resource_path);
    builder.append("data/profiles/");
    builder.append(profile);
    builder.append(".json");

    return config_open_json(builder.buf);
}

static bool init_with_profile(game_system* sys, svr::config* profile, const char* name)
{
    using namespace svr;

    auto root = config_root(profile);

    if (!load_movie_settings(sys, config_find(root, "movie"), name)) return false;
    if (!load_motion_blur_settings(sys, config_find(root, "motion-blur"))) return false;
    if (!load_preview_window_settings(sys, config_find(root, "preview-window"))) return false;

    if (sys->support_velocity_overlay)
    {
        if (!load_velocity_overlay_settings(sys, config_find(root, "velocity-overlay"))) return false;
    }

    return true;
}

static void process_overlays(game_system* sys)
{
    using namespace svr;

    if (sys->use_velocity_overlay)
    {
        auto vel = sqrt(sys->velocity.x * sys->velocity.x + sys->velocity.y * sys->velocity.y);

        char buf[128];
        auto res = std::to_chars(buf, buf + sizeof(buf), (int)(vel + 0.5f));
        *res.ptr = 0;

        auto p = sys->velocity_overlay_padding;

        sys->graphics->draw_text(sys->velocity_text_format, buf, 0 + p, 0 + p, sys->width - p, sys->height - p);
    }
}

// Converts the work texture into the destination pixel format. May use multiple planes.
// Then downloads all these planes into system memory and feeds it into the video encoder.
static void encode_video_frame(game_system* sys, svr::graphics_texture* tex)
{
    using namespace svr;

    process_overlays(sys);

    auto srv = sys->graphics->get_texture_srv(tex);

    graphics_texture* textures[3];
    sys->graphics->convert_pixel_formats(srv, sys->px_converter, textures, 3);

    mem_buffer buffers[3];

    for (size_t i = 0; i < sys->movie_plane_count; i++)
    {
        auto& b = buffers[i];
        mem_create_buffer(b, sys->movie_plane_sizes[i]);
        sys->graphics->download_texture(textures[i], b.data, b.size);
    }

    // Combine every plane into a single buffer and send it to the ffmpeg process.
    // If recording faster than realtime, this can crash the application due to 32 bit limitations.

    sys->encode_thread.run_task([=]() mutable
    {
        size_t offset = 0;

        for (size_t i = 0; i < sys->movie_plane_count; i++)
        {
            auto& b = buffers[i];
            memcpy((uint8_t*)sys->movie_buf.data + offset, b.data, sys->movie_plane_sizes[i]);
            offset += sys->movie_plane_sizes[i];
        }

        sys->movie->push_raw_video_data(sys->movie_buf.data, sys->movie_buf.size);

        for (size_t i = 0; i < sys->movie_plane_count; i++)
        {
            mem_destroy_buffer(buffers[i]);
        }
    });

    if (sys->use_preview_window)
    {
        sys->ui_preview->render(srv);
    }
}

static void transfer_game_to_work_texture(game_system* sys)
{
    // Draw the game content on the work texture.
    // All overlays will be added on top later.

    using namespace svr;

    auto srv = sys->graphics->get_texture_srv(sys->game_content);
    auto rtv = sys->graphics->get_texture_rtv(sys->work_texture);

    graphics_overlay_desc desc = {};
    desc.sampler_state = GRAPHICS_SAMPLER_POINT;
    desc.blend_state = GRAPHICS_BLEND_OPAQUE;
    desc.rect.w = sys->width;
    desc.rect.h = sys->height;

    sys->graphics->draw_overlay(srv, rtv, desc);
}

static void motion_sample_frame(game_system* sys)
{
    using namespace svr;

    graphics_motion_sample_desc desc = {};
    desc.width = sys->width;
    desc.height = sys->height;

    auto source_srv = sys->graphics->get_texture_srv(sys->game_content);
    auto dest_uav = sys->graphics->get_texture_uav(sys->work_texture);
    auto dest_rtv = sys->graphics->get_texture_rtv(sys->work_texture);

    auto old_rem = sys->sample_remainder;
    auto exposure = sys->sample_exposure;

    sys->sample_remainder += sys->sample_remainder_step;

    if (sys->sample_remainder <= (1.0f - exposure))
    {

    }

    else if (sys->sample_remainder < 1.0f)
    {
        auto weight = (sys->sample_remainder - std::max(1.0f - exposure, old_rem)) * (1.0f / exposure);
        sys->graphics->motion_sample(desc, source_srv, dest_uav, weight);
    }

    else
    {
        auto weight = (1.0f - std::max(1.0f - exposure, old_rem)) * (1.0f / exposure);
        sys->graphics->motion_sample(desc, source_srv, dest_uav, weight);

        encode_video_frame(sys, sys->work_texture);

        sys->sample_remainder -= 1.0f;

        uint32_t additional = sys->sample_remainder;

        if (additional > 0)
        {
            for (int i = 0; i < additional; i++)
            {
                encode_video_frame(sys, sys->work_texture);
            }

            sys->sample_remainder -= additional;
        }

        float clear_col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        sys->graphics->clear_rtv(sys->graphics->get_texture_rtv(sys->work_texture), clear_col);

        if (sys->sample_remainder > FLT_EPSILON && sys->sample_remainder > (1.0f - exposure))
        {
            weight = ((sys->sample_remainder - (1.0f - exposure)) * (1.0f / exposure));
            sys->graphics->motion_sample(desc, source_srv, dest_uav, weight);
        }
    }
}

game_system* sys_create(const char* resource_path, svr::graphics_backend* graphics)
{
    using namespace svr;

    auto sys = new game_system;
    sys->resource_path = resource_path;

    sys->graphics = graphics;
    sys->movie = movie_create_ffmpeg_pipe(resource_path);

    return sys;
}

void sys_destroy(game_system* sys)
{
    using namespace svr;

    graphics_destroy_backend(sys->graphics);
    movie_destroy(sys->movie);
    delete sys;
}

uint32_t sys_get_game_rate(game_system* sys)
{
    return sys->game_rate;
}

bool sys_open_shared_game_texture(game_system* sys, svr::os_handle* ptr)
{
    using namespace svr;

    graphics_texture_open_desc desc = {};
    desc.view_access = GRAPHICS_VIEW_SRV;

    sys->game_content = sys->graphics->open_shared_texture("sys game", ptr, desc);

    if (sys->game_content == nullptr)
    {
        log("Could not open shared game texture\n");
        return false;
    }

    log("Opened shared game texture\n");
    return true;
}

void sys_new_frame(game_system* sys)
{
    sys->frame_num++;

    // First frame will always be blank.

    if (sys->frame_num == 1)
    {
        return;
    }

    if (!sys->use_motion_blur)
    {
        transfer_game_to_work_texture(sys);
        encode_video_frame(sys, sys->work_texture);
        return;
    }

    motion_sample_frame(sys);
}

bool sys_movie_running(game_system* sys)
{
    return sys->movie_runing;
}

void sys_set_velocity_overlay_support(game_system* sys, bool value)
{
    sys->support_velocity_overlay = value;
}

bool sys_use_velocity_overlay(game_system* sys)
{
    return sys->use_velocity_overlay;
}

void sys_provide_velocity_overlay(game_system* sys, svr::vec3 v)
{
    sys->velocity = v;
}

bool sys_start_movie(game_system* sys, const char* name, const char* profile, uint32_t width, uint32_t height)
{
    using namespace svr;

    if (sys->movie_runing)
    {
        return false;
    }

    sys->width = width;
    sys->height = height;

    auto prof = load_profile(sys->resource_path, profile);

    if (prof == nullptr)
    {
        log("Could not find any matching profile\n");
        return false;
    }

    defer {
        config_destroy(prof);
    };

    if (!init_with_profile(sys, prof, name))
    {
        log("Could not initialize with profile '{}'\n", profile);
        return false;
    }

    // Make the work texture start blank.

    float clear_col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    sys->graphics->clear_rtv(sys->graphics->get_texture_rtv(sys->work_texture), clear_col);

    if (sys->use_preview_window)
    {
        sys->ui_thread.run_task([=]()
        {
            sys->ui_preview = graphics_preview_create_winapi(sys->graphics, width, height);
            ui_enter_message_loop();
            graphics_preview_destroy_winapi(sys->ui_preview);
            sys->ui_preview = nullptr;
        });
    }

    log("Starting movie to '{}'\n", name);

    sys->movie_runing = true;
    sys->frame_num = 0;

    sys->velocity = {};

    return true;
}

void sys_end_movie(game_system* sys)
{
    using namespace svr;

    if (!sys->movie_runing)
    {
        return;
    }

    log("Ending movie, waiting for encoder\n");

    // Keep things synchronous for simplicity.

    sys->encode_thread.run_task_wait([=]()
    {
        sys->movie->close_movie();
    });

    if (sys->use_preview_window)
    {
        ui_exit_message_loop(sys->ui_thread.get_thread_id());
    }

    sys->graphics->destroy_conversion_context(sys->px_converter);
    sys->px_converter = nullptr;

    if (sys->use_velocity_overlay)
    {
        sys->graphics->destroy_text_format(sys->velocity_text_format);
        sys->velocity_text_format = nullptr;
    }

    sys->graphics->destroy_texture(sys->work_texture);
    sys->work_texture = nullptr;

    sys->graphics->destroy_texture(sys->game_content);
    sys->game_content = nullptr;

    mem_destroy_buffer(sys->movie_buf);

    sys->movie_runing = false;

    log("Ended movie\n");
}
