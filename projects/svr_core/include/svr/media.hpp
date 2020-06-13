#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    enum media_type_flag
    {
        MEDIA_FLAG_NONE = 0,
        MEDIA_FLAG_VIDEO = 1u << 0,
        MEDIA_FLAG_AUDIO = 1u << 1,
    };

    using media_type_flag_t = uint32_t;

    enum media_pixel_format
    {
        MEDIA_PIX_FORMAT_NONE,
        MEDIA_PIX_FORMAT_BGR0,
        MEDIA_PIX_FORMAT_YUV420,
        MEDIA_PIX_FORMAT_NV12,
        MEDIA_PIX_FORMAT_NV21,
        MEDIA_PIX_FORMAT_YUV444,
    };

    enum media_color_space
    {
        MEDIA_COLOR_SPACE_NONE,
        MEDIA_COLOR_SPACE_RGB,
        MEDIA_COLOR_SPACE_YUV601,
        MEDIA_COLOR_SPACE_YUV709,
    };

    // Structure containing coefficients to convert RGB data into YUV.
    // Must be aligned to 16 bytes.
    struct media_yuv_color_space_matrix
    {
        float y[4];
        float u[4];
        float v[4];
    };

    // Returns how many planes there are in a pixel format.
    SVR_API size_t media_get_plane_count(media_pixel_format format);

    // Calculates the dimensions a plane has in a pixel format.
    SVR_API void media_calc_plane_dimensions(media_pixel_format format, size_t plane, int32_t width, int32_t height, int32_t* out_width, int32_t* out_height);

    // Returns the rgb->yuv conversion matrix for the given color space.
    // Not valid for rgb color space.
    SVR_API const media_yuv_color_space_matrix* media_get_color_space_matrix(media_color_space space);

    // Functions to provide readable names for media enumerations.

    SVR_API const char* media_pixel_format_to_string(media_pixel_format value);
    SVR_API media_pixel_format media_pixel_format_from_string(const char* value);

    SVR_API const char* media_color_space_to_string(media_color_space value);
    SVR_API media_color_space media_color_space_from_string(const char* value);
}
