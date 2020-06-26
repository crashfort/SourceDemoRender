#pragma once
#include <svr/api.hpp>
#include <svr/media.hpp>

#include <stdint.h>

namespace svr
{
    // Interface responsible for handling raw uncompressed streams
    // and encoding them accordingly into a media container.
    struct movie
    {
        virtual ~movie() = default;

        // Enables or disables verbose logging.
        virtual void set_log_enabled(bool value) = 0;

        // Attempts to open the movie with the current state.
        // Accesses the file system if everything works correctly and
        // raw data can start being sent.
        virtual bool open_movie() = 0;

        // Closes the movie.
        // Closing a movie means that no more frames can be accepted.
        // The movie is also closed on destruction if it wasn't closed beforehand.
        // Note that this may block for a considerable time if there are pending frames needing to be encoded and written.
        virtual void close_movie() = 0;

        // Sets the desired output file.
        // The media container is chosen from this path, so a valid container
        // should be supplied; such as mkv, mp4, avi.
        // The media container does not imply an automatic video / audio encoder is chosen.
        // Video encoder must be set with set_video_encoder.
        // Relative and absolute paths are allowed to be supplied. Only supplying a filename
        // implies the working directory of the client application.
        virtual void set_output(const char* path) = 0;

        // Sets the media mask to use for processing.
        // This function can be used to remove a certain type of media from processing.
        // The default value is only video.
        virtual void set_media_flags(media_type_flag_t value) = 0;

        // Sets the desired output video resolution of the media.
        // Values must be greater than 1.
        virtual void set_video_dimensions(int width, int height) = 0;

        // Sets the constant target framerate to use for output.
        // The default value is 60.
        virtual void set_video_fps(int value) = 0;

        // Sets the video encoder to use for encoding.
        // This value decides if the output is to be RGB video or YUV video.
        // The default value is libx264rgb.
        virtual void set_video_encoder(const char* value) = 0;

        // Sets the pixel format to use for encoding.
        // The input data for video is expected to be in this format.
        virtual void set_video_pixel_format(media_pixel_format value) = 0;

        // Sets the color space to use for encoding.
        // This is only metadata information.
        // The default value is MEDIA_COLOR_SPACE_RGB.
        virtual void set_video_color_space(media_color_space value) = 0;

        // Sets the amount of threads using for encoding.
        // A value of 0 means a value is automatically chosen from the hardware.
        // The default value is 0 (auto).
        virtual void set_threads(int value) = 0;

        // Sets the constant rate factor quality value.
        // A lower value means better quality with slower encoding speed.
        // The range of valid values is from 0 to 51.
        // The default value is 0.
        virtual void set_video_x264_crf(int value) = 0;

        // Sets the encoding speed preset.
        // This follows the standard library of x264 encoding names.
        // The default value is veryfast.
        virtual void set_video_x264_preset(const char* value) = 0;

        // Sets whether or not every frame will be a keyframe, disregarding any
        // inter frame compression.
        // The default value is true.
        virtual void set_video_x264_intra(bool value) = 0;

        // Pushes raw uncompressed video frame data.
        // The input format should follow the same pixel format as
        // is used in the movie.
        // For RGB video there will only be a single plane.
        // For YUV video there will be up to 3 planes.
        virtual void push_raw_video_data(void* data, size_t size) = 0;
    };

    // Creates a movie interface by piping data to an ffmpeg executable.
    SVR_API movie* movie_create_ffmpeg_pipe(const char* resource_path);

    // Destroys a movie.
    // Note that this may block for a considerable time if there are pending frames needing to be encoded and written.
    SVR_API void movie_destroy(movie* mov);
}
