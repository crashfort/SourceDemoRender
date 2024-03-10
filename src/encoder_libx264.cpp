#include "encoder_priv.h"

// References:
// ffmpeg -h encoder=libx264
// https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/libx264.c
// https://raw.githubusercontent.com/mirror/x264/master/x264.c

void EncoderState::render_setup_libx264()
{
    av_opt_set(render_video_ctx->priv_data, "preset", movie_params.x264_preset, 0);
    av_opt_set(render_video_ctx->priv_data, "crf", svr_va("%d", movie_params.x264_crf), 0);

    if (movie_params.x264_intra)
    {
        av_opt_set(render_video_ctx->priv_data, "x264-params", "keyint=1", 0);
    }
}
