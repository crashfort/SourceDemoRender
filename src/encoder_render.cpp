#include "encoder_priv.h"

// Actual calls to audio and video codecs and container.

const RenderVideoInfo RENDER_VIDEO_INFOS[] = {
    RenderVideoInfo { "dnxhr", "dnxhd", AV_PIX_FMT_YUV422P, &EncoderState::render_setup_dnxhr },
    RenderVideoInfo { "libx264", "libx264", AV_PIX_FMT_NV12, &EncoderState::render_setup_libx264 },
};

const RenderAudioInfo RENDER_AUDIO_INFOS[] = {
    RenderAudioInfo { "aac", "aac", AV_SAMPLE_FMT_FLTP, NULL },
};

bool EncoderState::render_init()
{
    return true;
}

bool EncoderState::render_start()
{
    assert(!render_started);

    bool ret = false;
    s32 res;

    if (!render_init_output_context())
    {
        goto rfail;
    }

    if (!render_init_video())
    {
        goto rfail;
    }

    if (movie_params.use_audio)
    {
        // Audio codec not able to be selected yet.
        if (!render_init_audio())
        {
            goto rfail;
        }
    }

    res = avformat_write_header(render_output_context, NULL);

    if (res < 0)
    {
        error("ERROR: Could not create render file header (%d)\n", res);
        goto rfail;
    }

    render_started = true;

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void EncoderState::render_free_static()
{
}

void EncoderState::render_free_dynamic()
{
    if (render_started)
    {
        // Flush out all of the remaining samples in the audio fifo.

        if (movie_params.use_audio)
        {
            s32 num_remaining = audio_num_queued_samples();

            while (num_remaining > 0)
            {
                s32 num_samples = svr_min(num_remaining, render_audio_frame_size);
                audio_copy_samples_to_frame(render_audio_frame, num_samples);

                render_audio_frame->pts = render_audio_pts;
                render_audio_frame->nb_samples = num_samples;
                render_audio_pts += num_samples;

                if (!render_encode_audio_frame(render_audio_frame))
                {
                    // Nothing to do in this case, just drop everything.
                    break;
                }

                num_remaining -= num_samples;
            }
        }

        // Flush the encoders if we don't have any error.
        // We cannot flush if we have errors because doing so would try and write additional packets to the container.
        // Writing even more would just make things worse, so instead bail and make a functional media file of what we've
        // already written, skipping the trailing packets.

        if (render_video_ctx)
        {
            render_encode_video_frame(NULL);
        }

        if (render_audio_ctx)
        {
            render_encode_audio_frame(NULL);
        }

        av_interleaved_write_frame(render_output_context, NULL);

        av_write_trailer(render_output_context); // Can only be written if avformat_write_header was called.
    }

    if (render_output_context)
    {
        avio_close(render_output_context->pb);

        avformat_free_context(render_output_context);
        render_output_context = NULL;
    }

    avcodec_free_context(&render_video_ctx);
    avcodec_free_context(&render_audio_ctx);

    av_frame_free(&render_video_frame);
    av_frame_free(&render_audio_frame);

    render_video_stream = NULL;
    render_audio_stream = NULL;

    render_container = NULL;

    render_started = false;

    render_video_pts = 0;
    render_audio_pts = 0;
}

// Find the structure matching the configuration in the movie profile.
bool EncoderState::render_setup_video_info()
{
    for (s32 i = 0; i < SVR_ARRAY_SIZE(RENDER_VIDEO_INFOS); i++)
    {
        const RenderVideoInfo* info = &RENDER_VIDEO_INFOS[i];

        if (!strcmp(info->profile_name, movie_params.video_encoder))
        {
            render_video_info = info;
            return true;
        }
    }

    error("ERROR: No video encoder was found with name %s\n", movie_params.video_encoder);
    return false;
}

// Find the structure matching the configuration in the movie profile.
bool EncoderState::render_setup_audio_info()
{
    for (s32 i = 0; i < SVR_ARRAY_SIZE(RENDER_AUDIO_INFOS); i++)
    {
        const RenderAudioInfo* info = &RENDER_AUDIO_INFOS[i];

        if (!strcmp(info->profile_name, movie_params.audio_encoder))
        {
            render_audio_info = info;
            return true;
        }
    }

    error("ERROR: No audio encoder was found with name %s\n", movie_params.audio_encoder);
    return false;
}

bool EncoderState::render_init_output_context()
{
    bool ret = false;

    // Guess container based on extension.
    render_container = av_guess_format(NULL, movie_params.dest_file, NULL);

    if (render_container == NULL)
    {
        error("ERROR: Could not find any possible container for rendering%s\n", movie_params.dest_file);
        goto rfail;
    }

    if (render_container->flags & AVFMT_NOFILE)
    {
        error("ERROR: Container %s is not for render file output\n", render_container->name);
        goto rfail;
    }

    s32 res = avformat_alloc_output_context2(&render_output_context, render_container, NULL, NULL);

    if (res < 0)
    {
        error("ERROR: Could not create render output context (%d)\n", res);
        goto rfail;
    }

    res = avio_open2(&render_output_context->pb, movie_params.dest_file, AVIO_FLAG_WRITE, NULL, NULL);

    if (res < 0)
    {
        error("ERROR: Could not create render output file (%d)\n", res);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::render_init_video()
{
    bool ret = false;
    s32 res;

    render_video_q = av_make_q(1, movie_params.video_fps);

    if (!render_setup_video_info())
    {
        goto rfail;
    }

    const AVCodec* codec = avcodec_find_encoder_by_name(render_video_info->codec_name);

    // Maybe seems silly but this is possible to happen if someone replaces the dlls or something.
    if (codec == NULL)
    {
        error("ERROR: No video encoder with name %s was found\n", render_video_info->codec_name);
        goto rfail;
    }

    res = avformat_query_codec(render_container, codec->id, FF_COMPLIANCE_EXPERIMENTAL);

    if (res <= 0)
    {
        error("ERROR: Encoder %s cannot be used in container %s (%d)\n", codec->name, render_container->name, res);
        goto rfail;
    }

    render_video_stream = avformat_new_stream(render_output_context, codec);

    if (render_video_stream == NULL)
    {
        error("ERROR: Could not create render video stream\n");
        goto rfail;
    }

    render_video_stream->id = render_output_context->nb_streams - 1;

    render_video_ctx = avcodec_alloc_context3(codec);

    if (render_video_ctx == NULL)
    {
        error("ERROR: Could not create video render codec context\n");
        goto rfail;
    }

    render_video_ctx->bit_rate = 0;
    render_video_ctx->width = movie_params.video_width;
    render_video_ctx->height = movie_params.video_height;
    render_video_ctx->time_base = render_video_q;
    render_video_ctx->pix_fmt = render_video_info->pixel_format;
    render_video_ctx->color_range = AVCOL_RANGE_MPEG;

    render_video_stream->time_base = render_video_ctx->time_base;
    render_video_stream->avg_frame_rate = av_inv_q(render_video_ctx->time_base);

    if (render_output_context->oformat->flags & AVFMT_GLOBALHEADER)
    {
        render_video_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (render_video_info->setup)
    {
        (this->*render_video_info->setup)();
    }

    res = avcodec_open2(render_video_ctx, codec, NULL);

    if (res < 0)
    {
        error("ERROR: Could not open render video codec (%d)\n", res);
        goto rfail;
    }

    render_video_frame = av_frame_alloc();

    if (render_video_frame == NULL)
    {
        error("ERROR: Could not create render video encode frame\n");
        goto rfail;
    }

    render_video_frame->format = render_video_ctx->pix_fmt;
    render_video_frame->width = render_video_ctx->width;
    render_video_frame->height = render_video_ctx->height;

    // Allocate buffers for frame.
    res = av_frame_get_buffer(render_video_frame, 0);

    if (res < 0)
    {
        error("ERROR: Could not allocate render video encode frame\n");
        goto rfail;
    }

    res = avcodec_parameters_from_context(render_video_stream->codecpar, render_video_ctx);

    if (res < 0)
    {
        error("ERROR: Could not transfer render video codec parameters to stream (%d)\n", res);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::render_init_audio()
{
    bool ret = false;
    s32 res;

    render_audio_q = av_make_q(1, movie_params.audio_hz);

    if (!render_setup_audio_info())
    {
        goto rfail;
    }

    const AVCodec* codec = avcodec_find_encoder_by_name(render_audio_info->codec_name);

    // Maybe seems silly but this is possible to happen if someone replaces the dlls or something.
    if (codec == NULL)
    {
        error("ERROR: No audio encoder with name %s was found\n", render_audio_info->codec_name);
        goto rfail;
    }

    res = avformat_query_codec(render_container, codec->id, FF_COMPLIANCE_EXPERIMENTAL);

    if (res <= 0)
    {
        error("ERROR: Encoder %s cannot be used in container %s (%d)\n", codec->name, render_container->name, res);
        goto rfail;
    }

    render_audio_stream = avformat_new_stream(render_output_context, codec);

    if (render_audio_stream == NULL)
    {
        error("ERROR: Could not create render audio stream\n");
        goto rfail;
    }

    render_audio_stream->id = render_output_context->nb_streams - 1;

    render_audio_ctx = avcodec_alloc_context3(codec);

    if (render_audio_ctx == NULL)
    {
        error("ERROR: Could not create audio render codec context\n");
        goto rfail;
    }

    AVChannelLayout channel_layout;
    av_channel_layout_default(&channel_layout, movie_params.audio_channels);

    render_audio_ctx->sample_fmt = render_audio_info->sample_format;
    render_audio_ctx->sample_rate = render_audio_q.den;
    render_audio_ctx->ch_layout = channel_layout;
    render_audio_ctx->time_base = render_audio_q;

    render_audio_stream->time_base = render_audio_ctx->time_base;

    if (render_output_context->oformat->flags & AVFMT_GLOBALHEADER)
    {
        render_audio_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (render_audio_info->setup)
    {
        (this->*render_audio_info->setup)();
    }

    res = avcodec_open2(render_audio_ctx, codec, NULL);

    if (res < 0)
    {
        error("ERROR: Could not open render audio codec (%d)\n", res);
        goto rfail;
    }

    // In case the encoder doesn't report how many samples it wants, just pick a number of samples that we want.
    if (codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    {
        render_audio_frame_size = 512;
    }

    else
    {
        render_audio_frame_size = render_audio_ctx->frame_size;
    }

    render_audio_frame = av_frame_alloc();

    if (render_audio_frame == NULL)
    {
        error("ERROR: Could not create render audio encode frame\n");
        goto rfail;
    }

    render_audio_frame->format = render_audio_ctx->sample_fmt;
    render_audio_frame->ch_layout = render_audio_ctx->ch_layout;
    render_audio_frame->sample_rate = render_audio_ctx->sample_rate;
    render_audio_frame->nb_samples = render_audio_frame_size; // All submitted audio frames will have this amount of samples, except the last.
    render_audio_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // Allocate buffers for frame.
    res = av_frame_get_buffer(render_audio_frame, 0);

    if (res < 0)
    {
        error("ERROR: Could not allocate render audio encode frame\n");
        goto rfail;
    }

    res = avcodec_parameters_from_context(render_audio_stream->codecpar, render_audio_ctx);

    if (res < 0)
    {
        error("ERROR: Could not transfer render audio codec parameters to stream (%d)\n", res);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    av_channel_layout_uninit(&channel_layout);
    return ret;
}

// The shared game texture has been updated at this point.
bool EncoderState::render_receive_video()
{
    assert(render_started);

    bool ret = false;

    vid_convert_to_codec_textures(render_video_frame);

    render_video_frame->pts = render_video_pts;

    if (!render_encode_video_frame(render_video_frame))
    {
        goto rfail;
    }

    render_video_pts++;

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

// The shared audio samples have been updated at this point.
bool EncoderState::render_receive_audio()
{
    assert(render_started);
    assert(movie_params.use_audio);
    assert(shared_mem_ptr->waiting_audio_samples > 0);

    bool ret = false;

    audio_convert_to_codec_samples();

    if (!audio_have_enough_samples_to_encode())
    {
        ret = true;
        goto rexit;
    }

    audio_copy_samples_to_frame(render_audio_frame, render_audio_frame_size);

    render_audio_frame->pts = render_audio_pts;
    render_audio_frame->nb_samples = render_audio_frame_size;
    render_audio_pts += render_audio_frame_size;

    if (!render_encode_audio_frame(render_audio_frame))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::render_encode_video_frame(AVFrame* frame)
{
    return render_encode_frame(render_video_ctx, render_video_stream, frame);
}

bool EncoderState::render_encode_audio_frame(AVFrame* frame)
{
    return render_encode_frame(render_audio_ctx, render_audio_stream, frame);
}

bool EncoderState::render_encode_frame(AVCodecContext* ctx, AVStream* stream, AVFrame* frame)
{
    bool ret = false;
    s32 res;

    const auto test = AVERROR(EINVAL);

    res = avcodec_send_frame(ctx, frame);

    if (res < 0)
    {
        error("ERROR: Could not send raw frame to encoder (%d)\n", res);
        goto rfail;
    }

    while (!res)
    {
        AVPacket* packet = av_packet_alloc();

        res = avcodec_receive_packet(ctx, packet);

        // This will return AVERROR(EAGAIN) when we need to send more data.
        // This will return AVERROR_EOF when we are sending a flush frame.
        if (res == AVERROR(EAGAIN) || res == AVERROR_EOF)
        {
            // TODO Should not allocate and then free the packet like this. Possibly store
            // a packet and then ref/deref instead.
            av_packet_free(&packet);

            ret = true;
            goto rexit;
        }

        if (res < 0)
        {
            error("ERROR: Could not receive packet from encoder (%d)\n", res);
            av_packet_free(&packet);
            goto rfail;
        }

        if (res == 0)
        {
            packet->pts = av_rescale_q(packet->pts, ctx->time_base, stream->time_base);
            packet->dts = av_rescale_q(packet->dts, ctx->time_base, stream->time_base);
            packet->duration = av_rescale_q(packet->duration, ctx->time_base, stream->time_base);
            packet->stream_index = stream->index;

            res = av_interleaved_write_frame(render_output_context, packet);

            av_packet_free(&packet);

            if (res < 0)
            {
                error("ERROR: Could not write encoded packet to container (%d)\n", res);
                goto rfail;
            }
        }
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}
