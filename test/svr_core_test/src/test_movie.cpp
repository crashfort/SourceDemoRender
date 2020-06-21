#include <catch2/catch.hpp>

#include <svr/graphics.hpp>
#include <svr/movie.hpp>
#include <svr/os.hpp>
#include <svr/str.hpp>
#include <svr/defer.hpp>
#include <svr/media.hpp>
#include <svr/mem.hpp>

static const auto WIDTH = 1024;
static const auto HEIGHT = 1024;

static const char* CONTAINERS[] = {
    ".mp4",
    ".mkv",
};

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

static void create_movie(const char* ext, const char* codec, svr::media_pixel_format px, svr::media_color_space space)
{
    using namespace svr;

    char cur_dir[512];
    REQUIRE(os_get_current_dir(cur_dir, sizeof(cur_dir)));

    auto mov = movie_create_ffmpeg_pipe(cur_dir);
    REQUIRE(mov);

    defer {
        movie_destroy(mov);
    };

    mov->set_log_enabled(false);
    mov->set_threads(0);
    mov->set_video_fps(6);
    mov->set_video_encoder(codec);
    mov->set_video_pixel_format(px);
    mov->set_video_color_space(space);
    mov->set_video_x264_crf(23);
    mov->set_video_x264_preset("veryfast");
    mov->set_video_x264_intra(false);

    str_builder builder;
    builder.append(cur_dir);
    builder.append("data/movies/");
    builder.append(media_pixel_format_to_string(px));
    builder.append("_");
    builder.append(media_color_space_to_string(space));
    builder.append(ext);

    mov->set_output(builder.buf);
    mov->set_video_dimensions(WIDTH, HEIGHT);

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

    graphics_conversion_context_desc conv_desc = {};
    conv_desc.width = WIDTH;
    conv_desc.height = HEIGHT;
    conv_desc.source_format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    conv_desc.dest_format = px;
    conv_desc.dest_color_space = space;

    auto conv = graphics->create_conversion_context("conv", conv_desc);
    REQUIRE(conv);

    mem_buffer movie_buf;
    size_t total_size = 0;
    size_t movie_plane_sizes[3];

    auto movie_plane_count = graphics->get_conversion_texture_count(conv);
    graphics->get_conversion_sizes(conv, movie_plane_sizes, 3);

    for (size_t i = 0; i < movie_plane_count; i++)
    {
        total_size += movie_plane_sizes[i];
    }

    mem_create_buffer(movie_buf, total_size);

    REQUIRE(mov->open_movie());

    for (size_t i = 0; i < 6; i++)
    {
        auto col = CLEAR_COLORS[i];

        float asd[] = { col.r, col.g, col.b, col.a };

        graphics->clear_rtv(tex_rtv, asd);

        graphics_texture* textures[3];
        graphics->convert_pixel_formats(tex_srv, conv, textures, 3);

        // Split in two parts because of how it's actually done in game_system due to threading.

        mem_buffer buffers[3];

        for (size_t i = 0; i < movie_plane_count; i++)
        {
            auto& b = buffers[i];
            mem_create_buffer(b, movie_plane_sizes[i]);
            graphics->download_texture(textures[i], b.data, b.size);
        }

        size_t offset = 0;

        for (size_t i = 0; i < movie_plane_count; i++)
        {
            auto& b = buffers[i];
            memcpy((uint8_t*)movie_buf.data + offset, b.data, movie_plane_sizes[i]);
            offset += movie_plane_sizes[i];
        }

        mov->push_raw_video_data(movie_buf.data, movie_buf.size);

        for (size_t i = 0; i < movie_plane_count; i++)
        {
            mem_destroy_buffer(buffers[i]);
        }
    }

    mov->close_movie();
}

TEST_CASE("create movies")
{
    using namespace svr;

    for (auto c : CONTAINERS)
    {
        create_movie(c, "libx264rgb", MEDIA_PIX_FORMAT_BGR0, MEDIA_COLOR_SPACE_RGB);

        create_movie(c, "libx264", MEDIA_PIX_FORMAT_YUV420, MEDIA_COLOR_SPACE_YUV601);
        create_movie(c, "libx264", MEDIA_PIX_FORMAT_YUV420, MEDIA_COLOR_SPACE_YUV709);

        create_movie(c, "libx264", MEDIA_PIX_FORMAT_NV12, MEDIA_COLOR_SPACE_YUV601);
        create_movie(c, "libx264", MEDIA_PIX_FORMAT_NV12, MEDIA_COLOR_SPACE_YUV709);

        create_movie(c, "libx264", MEDIA_PIX_FORMAT_NV21, MEDIA_COLOR_SPACE_YUV601);
        create_movie(c, "libx264", MEDIA_PIX_FORMAT_NV21, MEDIA_COLOR_SPACE_YUV709);

        create_movie(c, "libx264", MEDIA_PIX_FORMAT_YUV444, MEDIA_COLOR_SPACE_YUV601);
        create_movie(c, "libx264", MEDIA_PIX_FORMAT_YUV444, MEDIA_COLOR_SPACE_YUV709);
    }
}
