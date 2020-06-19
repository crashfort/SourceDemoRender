#include <svr/movie.hpp>
#include <svr/log_format.hpp>
#include <svr/os.hpp>
#include <svr/str.hpp>
#include <svr/defer.hpp>
#include <svr/platform.hpp>

#include <assert.h>
#include <algorithm>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

static const char* map_ffmpeg_pixel_format(svr::media_pixel_format value)
{
    using namespace svr;

    switch (value)
    {
        case MEDIA_PIX_FORMAT_BGR0: return "bgr0";
        case MEDIA_PIX_FORMAT_YUV420: return "yuv420p";
        case MEDIA_PIX_FORMAT_NV12: return "nv12";
        case MEDIA_PIX_FORMAT_NV21: return "nv21";
        case MEDIA_PIX_FORMAT_YUV444: return "yuv444p";
    }

    assert(false);
    return nullptr;
}

// https://ffmpeg.org/ffmpeg-all.html#toc-colorspace
static const char* map_ffmpeg_color_space(svr::media_color_space value)
{
    using namespace svr;

    switch (value)
    {
        case MEDIA_COLOR_SPACE_YUV601: return "bt470bg";
        case MEDIA_COLOR_SPACE_YUV709: return "bt709";
    }

    assert(false);
    return nullptr;
}

struct movie_ffmpeg_pipe
    : svr::movie
{
    ~movie_ffmpeg_pipe()
    {
        close_movie();
    }

    void set_log_enabled(bool value) override
    {
        log_enabled = value;
    }

    bool open_movie() override
    {
        using namespace svr;

        if (is_open)
        {
            log("Movie is already open\n");
            return false;
        }

        log_movie_details();

        if (!verify_movie())
        {
            log("Could not verify movie\n");
            return false;
        }

        str_builder exe;
        exe.append(resource_path);
        exe.append("ffmpeg");
        exe.append(platform_exe());

        auto args = build_ffmpeg_args(resource_path);

        log("Using ffmpeg args '{}'\n", args.buf);

        // Keep the writing end of the pipe, pass the reading end to the new process.

        os_handle* pipe_read = nullptr;

        defer {
            if (pipe_read) os_close_handle(pipe_read);
        };

        if (!os_create_pipe_pair(&pipe_read, &ff_pipe_write))
        {
            log("Could not create com pipe\n");
            return false;
        }

        os_start_proc_desc desc = {};
        desc.hide_window = !log_enabled;
        desc.input_pipe = pipe_read;

        if (!os_start_proc(exe.buf, resource_path, args.buf, &desc, &ff_proc, nullptr))
        {
            log("Could not open movie\n");
            return false;
        }

        log("Started ffmpeg process with id {}\n", os_get_proc_id(ff_proc));

        is_open = true;
        return true;
    }

    void close_movie() override
    {
        using namespace svr;

        if (!is_open)
        {
            return;
        }

        if (ff_pipe_write)
        {
            // Close our end of the pipe.
            // This will mark the completion of the stream, and the process will finish its work.
            os_close_handle(ff_pipe_write);
            ff_pipe_write = nullptr;
        }

        if (ff_proc)
        {
            // We have marked the stream as finished, but still have to wait for the process to finish.
            os_handle_wait(ff_proc, -1);

            os_close_handle(ff_proc);
            ff_proc = nullptr;
        }

        is_open = false;
    }

    void set_output(const char* path) override
    {
        using namespace svr;

        auto len = std::min(strlen(path), sizeof(output_path) - 1);
        memcpy(output_path, path, len);
        output_path[len] = 0;

        os_normalize_path(output_path, len);
    }

    void set_media_flags(svr::media_type_flag_t value) override
    {
        using namespace svr;

        media_flags = value;
        media_flags |= MEDIA_FLAG_VIDEO;
    }

    void set_video_dimensions(int width, int height) override
    {
        video_width = std::max(2, width);
        video_height = std::max(2, height);
    }

    void set_video_fps(int value) override
    {
        video_fps = std::max(1, value);
    }

    void set_video_encoder(const char* value) override
    {
        video_codec = value;
    }

    void set_video_pixel_format(svr::media_pixel_format value) override
    {
        video_pixel_format = value;
    }

    void set_video_color_space(svr::media_color_space value) override
    {
        video_color_space = value;
    }

    void set_threads(int value) override
    {
        threads = std::max(0, value);
    }

    void set_video_x264_crf(int value) override
    {
        video_x264_crf = value;
    }

    void set_video_x264_preset(const char* value) override
    {
        auto len = std::min(strlen(value), sizeof(video_x264_preset) - 1);
        memcpy(video_x264_preset, value, len);
        video_x264_preset[len] = 0;
    }

    void set_video_x264_intra(bool value) override
    {
        video_x264_intra = value;
    }

    void push_raw_video_data(void* data, size_t size) override
    {
        using namespace svr;

        auto res = os_write_pipe(ff_pipe_write, data, size);

        if (res == false)
        {
            log("Could not write {} bytes to movie pipe. Has process {} ended?\n", size, os_get_proc_id(ff_proc));
        }
    }

    void log_movie_details()
    {
        using namespace svr;

        log("Trying to open movie:\n");
        log("  - Video flag: {}\n", media_flags & MEDIA_FLAG_VIDEO);
        log("  - Encoding threads: {}\n", threads);

        if (media_flags & MEDIA_FLAG_VIDEO)
        {
            log("  - Video resolution: {}x{}\n", video_width, video_height);
            log("  - Video fps: {}\n", video_fps);
            log("  - Video encoder: {}\n", video_codec);
            log("  - Video pixel format: {}\n", media_pixel_format_to_string(video_pixel_format));
            log("  - Video color space: {}\n", media_color_space_to_string(video_color_space));
            log("  - Video x264 crf: {}\n", video_x264_crf);
            log("  - Video x264 preset: {}\n", video_x264_preset);
            log("  - Video x264 intra: {}\n", video_x264_intra);
        }
    }

    bool verify_movie()
    {
        using namespace svr;

        // TODO
        // Remove this when vcpkg works.
        return true;

        auto codec = avcodec_find_encoder_by_name(video_codec);
        auto format = av_guess_format(nullptr, output_path, nullptr);

        if (format == nullptr || (format && format->video_codec == AV_CODEC_ID_NONE))
        {
            log("No supported container format\n");
            return false;
        }

        if (codec == nullptr)
        {
            log("Could not find any encoder with name '{}'\n", video_codec);
            return false;
        }

        if (video_pixel_format == MEDIA_PIX_FORMAT_NONE)
        {
            log("No pixel format is set\n");
            return false;
        }

        if (video_color_space == MEDIA_COLOR_SPACE_NONE)
        {
            log("No color space is set\n");
            return false;
        }

        log("Using container '{}'\n", format->long_name);
        log("Using codec '{}'\n", codec->long_name);

        return true;
    }

    svr::str_builder build_ffmpeg_args(const char* resource_path)
    {
        using namespace svr;

        str_builder builder;
        builder.append("-hide_banner");

        if (log_enabled)
        {
            builder.append(" -loglevel debug");
        }

        else
        {
            builder.append(" -loglevel quiet");
        }

        // We are sending raw uncompressed data.
        builder.append(" -f rawvideo -vcodec rawvideo");

        fmt::memory_buffer buf;

        // Input dimensions.
        format_with_null(buf, " -s {}x{}", video_width, video_height);
        builder.append(buf.data());

        // Input raw pixel format.
        format_with_null(buf, " -pix_fmt {}", map_ffmpeg_pixel_format(video_pixel_format));
        builder.append(buf.data());

        // Input framerate.
        format_with_null(buf, " -r {}", video_fps);
        builder.append(buf.data());

        // Overwrite existing, and read from stdin.
        builder.append(" -y -i -");

        // Number of encoding threads, or 0 for auto.
        format_with_null(buf, " -threads {}", threads);
        builder.append(buf.data());

        // Output video codec.
        format_with_null(buf, " -vcodec {}", video_codec);
        builder.append(buf.data());

        if (video_color_space != MEDIA_COLOR_SPACE_RGB)
        {
            // Output video color space.
            format_with_null(buf, " -colorspace {}", map_ffmpeg_color_space(video_color_space));
            builder.append(buf.data());
        }

        // Output video framerate.
        format_with_null(buf, " -framerate {}", video_fps);
        builder.append(buf.data());

        // Output constant rate factor.
        format_with_null(buf, " -crf {}", video_x264_crf);
        builder.append(buf.data());

        // Output x264 preset.
        format_with_null(buf, " -preset {}", video_x264_preset);
        builder.append(buf.data());

        // Make every frame a keyframe.
        if (video_x264_intra)
        {
            builder.append(" -x264-params keyint=1");
        }

        builder.append(" \"");
        builder.append(output_path);
        builder.append("\"");

        return builder;
    }

    const char* resource_path;

    bool is_open = false;
    bool log_enabled = false;

    // The resulting file which will be created and written to.
    // The container is also decided from this.
    char output_path[512];

    svr::media_type_flag_t media_flags = svr::MEDIA_FLAG_VIDEO;

    int video_width = 0;
    int video_height = 0;

    int video_fps;

    const char* video_codec;
    svr::media_pixel_format video_pixel_format;
    svr::media_color_space video_color_space;

    int threads;

    int video_x264_crf;
    char video_x264_preset[32];
    bool video_x264_intra;

    svr::os_handle* ff_pipe_write = nullptr;
    svr::os_handle* ff_proc = nullptr;
};

namespace svr
{
    movie* movie_create_ffmpeg_pipe(const char* resource_path)
    {
        auto ret = new movie_ffmpeg_pipe;
        ret->resource_path = resource_path;

        return ret;
    }

    void movie_destroy(movie* mov)
    {
        delete mov;
    }
}
