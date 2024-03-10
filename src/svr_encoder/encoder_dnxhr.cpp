#include "encoder_priv.h"

// References:
// ffmpeg -h encoder=dnxhd
// https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/dnxhdenc.c
// https://resources.avid.com/SupportFiles/attach/HighRes_WorkflowsGuide.pdf

void EncoderState::render_setup_dnxhr()
{
    // In the profile ini we just write hq, lb or sq, but ffmpeg needs them to be prefixed with dnxhr_.
    char buf[32];
    snprintf(buf, SVR_ARRAY_SIZE(buf), "dnxhr_%s", movie_params.dnxhr_profile);

    av_opt_set(render_video_ctx->priv_data, "profile", buf, 0);
}
