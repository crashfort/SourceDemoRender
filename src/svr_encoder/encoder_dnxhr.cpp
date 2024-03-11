#include "encoder_priv.h"

// References:
// ffmpeg -h encoder=dnxhd
// https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/dnxhdenc.c
// https://resources.avid.com/SupportFiles/attach/HighRes_WorkflowsGuide.pdf

void EncoderState::render_setup_dnxhr()
{
    // In the profile ini we just write hq, lb or sq, but ffmpeg needs them to be prefixed with dnxhr_.
    av_opt_set(render_video_ctx->priv_data, "profile", svr_va("dnxhr_%s", movie_params.dnxhr_profile), 0);

    render_video_ctx->thread_type = FF_THREAD_SLICE; // Crashes without this.
}
