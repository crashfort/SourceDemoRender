#include <svr/media.hpp>
#include <svr/table.hpp>

extern "C"
{
    #include <libavutil/imgutils.h>
}

#include <assert.h>

static auto make_matrix(float _00, float _10, float _20,
                        float _01, float _11, float _21,
                        float _02, float _12, float _22)
{
    svr::media_yuv_color_space_matrix ret = {
        { _00, _10, _20, 0 },
        { _01, _11, _21, 0 },
        { _02, _12, _22, 0 },
    };

    return ret;
}

static auto YUV_601_COLOR_SPACE_MATRIX = make_matrix(+0.299000f, +0.587000f, +0.114000f,
                                                     -0.168736f, -0.331264f, +0.500000f,
                                                     +0.500000f, -0.418688f, -0.081312f);

static auto YUV_709_COLOR_SPACE_MATRIX = make_matrix(+0.212600f, +0.715200f, +0.072200f,
                                                     -0.114572f, -0.385428f, +0.500000f,
                                                     +0.500000f, -0.454153f, -0.045847f);

static svr::table PIXEL_FORMATS = {
    svr::table_pair{"none", svr::MEDIA_PIX_FORMAT_NONE},
    svr::table_pair{"bgr0", svr::MEDIA_PIX_FORMAT_BGR0},
    svr::table_pair{"yuv420", svr::MEDIA_PIX_FORMAT_YUV420},
    svr::table_pair{"nv12", svr::MEDIA_PIX_FORMAT_NV12},
    svr::table_pair{"nv21", svr::MEDIA_PIX_FORMAT_NV21},
    svr::table_pair{"yuv444", svr::MEDIA_PIX_FORMAT_YUV444},
};

static svr::table COLOR_SPACES = {
    svr::table_pair{"none", svr::MEDIA_COLOR_SPACE_NONE},
    svr::table_pair{"rgb", svr::MEDIA_COLOR_SPACE_RGB},
    svr::table_pair{"601", svr::MEDIA_COLOR_SPACE_YUV601},
    svr::table_pair{"709", svr::MEDIA_COLOR_SPACE_YUV709},
};

static AVPixelFormat convert_pixel_format(svr::media_pixel_format value)
{
    switch (value)
    {
        case svr::MEDIA_PIX_FORMAT_BGR0:
        {
            return AV_PIX_FMT_BGR0;
        }

        case svr::MEDIA_PIX_FORMAT_YUV420:
        {
            return AV_PIX_FMT_YUV420P;
        }

        case svr::MEDIA_PIX_FORMAT_NV12:
        {
            return AV_PIX_FMT_NV12;
        }

        case svr::MEDIA_PIX_FORMAT_NV21:
        {
            return AV_PIX_FMT_NV21;
        }

        case svr::MEDIA_PIX_FORMAT_YUV444:
        {
            return AV_PIX_FMT_YUV444P;
        }
    }

    assert(false);
    return AV_PIX_FMT_NONE;
}

namespace svr
{
    size_t media_get_plane_count(media_pixel_format format)
    {
        return av_pix_fmt_count_planes(convert_pixel_format(format));
    }

    void media_calc_plane_dimensions(media_pixel_format format, size_t plane, int32_t width, int32_t height, int32_t* out_width, int32_t* out_height)
    {
        auto converted_format = convert_pixel_format(format);

        width = av_image_get_linesize(converted_format, width, plane);

        if (plane > 0)
        {
            auto desc = av_pix_fmt_desc_get(converted_format);
            height = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);
        }

        *out_width = width;
        *out_height = height;
    }

    const media_yuv_color_space_matrix* media_get_color_space_matrix(media_color_space space)
    {
        switch (space)
        {
            case MEDIA_COLOR_SPACE_YUV601:
            {
                return &YUV_601_COLOR_SPACE_MATRIX;
            }

            case MEDIA_COLOR_SPACE_YUV709:
            {
                return &YUV_709_COLOR_SPACE_MATRIX;
            }
        }

        return nullptr;
    }

    const char* media_pixel_format_to_string(media_pixel_format value)
    {
        return table_map_value_or(PIXEL_FORMATS, value, "none");
    }

    media_pixel_format media_pixel_format_from_string(const char* value)
    {
        return table_map_key_or(PIXEL_FORMATS, value, MEDIA_PIX_FORMAT_NONE);
    }

    const char* media_color_space_to_string(media_color_space value)
    {
        return table_map_value_or(COLOR_SPACES, value, "none");
    }

    media_color_space media_color_space_from_string(const char* value)
    {
        return table_map_key_or(COLOR_SPACES, value, MEDIA_COLOR_SPACE_NONE);
    }
}
