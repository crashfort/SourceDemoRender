#include "encoder_priv.h"

// Actual calls to audio and video codecs and container.

const RenderVideoInfo RENDER_VIDEO_INFOS[] = {
    RenderVideoInfo { "dnxhr", "dnxhd", AV_PIX_FMT_YUV422P, &EncoderState::render_setup_dnxhr },
    RenderVideoInfo { "libx264", "libx264", AV_PIX_FMT_NV12, &EncoderState::render_setup_libx264 },
};

const RenderAudioInfo RENDER_AUDIO_INFOS[] = {
    RenderAudioInfo { "aac", "aac_mf", AV_SAMPLE_FMT_S16, 0, NULL },
};

DWORD CALLBACK render_frame_thread_proc(LPVOID param)
{
    SetThreadDescription(GetCurrentThread(), L"RENDER FRAME THREAD");

    EncoderState* encoder_ptr = (EncoderState*)param;
    encoder_ptr->render_frame_proc();

    return 0; // Not used.
}

DWORD CALLBACK render_packet_thread_proc(LPVOID param)
{
    SetThreadDescription(GetCurrentThread(), L"RENDER PACKET THREAD");

    EncoderState* encoder_ptr = (EncoderState*)param;
    encoder_ptr->render_packet_proc();

    return 0; // Not used.
}

bool EncoderState::render_init()
{
    render_frame_queue.init(RENDER_QUEUED_FRAMES);
    render_packet_queue.init(RENDER_QUEUED_PACKETS);
    render_recycled_video_frames.init(RENDER_QUEUED_FRAMES);
    render_recycled_audio_frames.init(RENDER_QUEUED_FRAMES);

    render_frame_wake_event_h = CreateEventA(NULL, FALSE, FALSE, NULL);
    render_packet_wake_event_h = CreateEventA(NULL, FALSE, FALSE, NULL);

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

    // Threads are ok at the start.
    svr_atom_store(&render_frame_thread_status, 1);
    svr_atom_store(&render_packet_thread_status, 1);

    render_frame_thread_message[0] = 0;
    render_packet_thread_message[0] = 0;

    // Be extra sure that these events are not triggered, so the threads enter a waiting state.
    ResetEvent(render_frame_wake_event_h);
    ResetEvent(render_packet_wake_event_h);

    render_frame_thread_h = CreateThread(NULL, 0, render_frame_thread_proc, this, 0, NULL);
    render_packet_thread_h = CreateThread(NULL, 0, render_packet_thread_proc, this, 0, NULL);

    render_started = true;

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void EncoderState::render_free_static()
{
    if (render_frame_wake_event_h)
    {
        CloseHandle(render_frame_wake_event_h);
        render_frame_wake_event_h = NULL;
    }

    if (render_packet_wake_event_h)
    {
        CloseHandle(render_packet_wake_event_h);
        render_packet_wake_event_h = NULL;
    }

    render_frame_queue.free();
    render_packet_queue.free();
    render_recycled_video_frames.free();
    render_recycled_audio_frames.free();
}

void EncoderState::render_free_dynamic()
{
    if (render_started)
    {
        // Flush out all of the remaining samples in the audio fifo for encode.

        if (movie_params.use_audio)
        {
            render_flush_audio_fifo();
        }

        // Send flushes to frame thread.

        if (render_video_ctx)
        {
            render_encode_video_frame(NULL);
        }

        if (render_audio_ctx)
        {
            render_encode_audio_frame(NULL);
        }

        WaitForSingleObject(render_frame_thread_h, INFINITE); // Wait for frame thread to finish.

        // Flush the packet thread.

        AVPacket* flush_packet = NULL;
        render_packet_queue.push(&flush_packet);
        SetEvent(render_packet_wake_event_h); // Notify packet thread.

        WaitForSingleObject(render_packet_thread_h, INFINITE); // Wait for packet thread to finish.

        CloseHandle(render_frame_thread_h);
        render_frame_thread_h = NULL;

        CloseHandle(render_packet_thread_h);
        render_packet_thread_h = NULL;

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

    render_video_stream = NULL;
    render_audio_stream = NULL;

    render_video_info = NULL;
    render_audio_info = NULL;

    render_container = NULL;

    render_started = false;

    render_video_pts = 0;
    render_audio_pts = 0;
}

// In frame thread.
void EncoderState::render_frame_proc()
{
    bool run = true;

    while (run)
    {
        WaitForSingleObject(render_frame_wake_event_h, INFINITE);

        RenderFrameThreadInput input = {};

        while (render_frame_queue.pull(&input))
        {
            if (input.frame == NULL)
            {
                run = false; // Stop on flush frame.
            }

            s32 res = avcodec_send_frame(input.ctx, input.frame);

            // Recycle frames.
            // We don't want to allocate big frames if we don't have to.
            // Flush frame must not be reused.
            if (input.frame)
            {
                if (input.type == AVMEDIA_TYPE_VIDEO)
                {
                    render_recycled_video_frames.push(&input.frame);
                }

                if (input.type == AVMEDIA_TYPE_AUDIO)
                {
                    render_recycled_audio_frames.push(&input.frame);
                }
            }

            if (res < 0)
            {
                SVR_SNPRINTF(render_frame_thread_message, "ERROR: Could not send raw frame to encoder (%d)\n", res);
                goto rfail;
            }

            while (res == 0)
            {
                AVPacket* packet = av_packet_alloc();

                res = avcodec_receive_packet(input.ctx, packet);

                // This will return AVERROR(EAGAIN) when we need to send more data.
                // This will return AVERROR_EOF when we are sending a flush frame.
                if (res == AVERROR(EAGAIN) || res == AVERROR_EOF)
                {
                    // TODO Should not allocate and then free the packet like this.
                    av_packet_free(&packet);
                    break;
                }

                if (res < 0)
                {
                    SVR_SNPRINTF(render_frame_thread_message, "ERROR: Could not receive packet from encoder (%d)\n", res);
                    av_packet_free(&packet);
                    goto rfail;
                }

                if (res == 0)
                {
                    packet->pts = av_rescale_q(packet->pts, input.ctx->time_base, input.stream->time_base);
                    packet->dts = av_rescale_q(packet->dts, input.ctx->time_base, input.stream->time_base);
                    packet->duration = av_rescale_q(packet->duration, input.ctx->time_base, input.stream->time_base);
                    packet->stream_index = input.stream->index;

                    // Send to packet thread.
                    render_packet_queue.push(&packet);
                    SetEvent(render_packet_wake_event_h); // Notify packet thread.
                }
            }
        }
    }

    goto rexit;

rfail:
    svr_atom_store(&render_frame_thread_status, 0);

rexit:
    return;
}

// In packet thread.
void EncoderState::render_packet_proc()
{
    bool run = true;

    while (run)
    {
        WaitForSingleObject(render_packet_wake_event_h, INFINITE);

        AVPacket* packet = NULL;

        while (render_packet_queue.pull(&packet))
        {
            if (packet == NULL)
            {
                run = false; // Stop on flush packet.
            }

            s32 res = av_interleaved_write_frame(render_output_context, packet);

            av_packet_free(&packet);

            if (res < 0)
            {
                SVR_SNPRINTF(render_packet_thread_message, "ERROR: Could not write encoded packet to container (%d)\n", res);
                goto rfail;
            }
        }
    }

    goto rexit;

rfail:
    svr_atom_store(&render_packet_thread_status, 0);

rexit:
    return;
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

    if (!render_setup_video_info())
    {
        goto rfail;
    }

    // Time base for video. Always based in seconds, so 1/60 for example.
    AVRational video_q = av_make_q(1, movie_params.video_fps);

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
    render_video_ctx->time_base = video_q;
    render_video_ctx->pix_fmt = render_video_info->pixel_format;
    render_video_ctx->color_range = AVCOL_RANGE_MPEG;

    render_video_stream->time_base = render_video_ctx->time_base;
    render_video_stream->avg_frame_rate = av_inv_q(render_video_ctx->time_base);

    if (render_output_context->oformat->flags & AVFMT_GLOBALHEADER)
    {
        render_video_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    render_video_ctx->thread_count = 0; // Use all threads.

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

    // Preallocate some frames.

    for (s32 i = 0; i < 8; i++)
    {
        render_get_new_video_frame();
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

    if (!render_setup_audio_info())
    {
        goto rfail;
    }

    s32 hz = movie_params.audio_hz;

    // Set from encoder if it requires a set sample rate.
    if (render_audio_info->hz != 0)
    {
        hz = render_audio_info->hz;
    }

    // Time base for video. Always based in seconds, so 1/44100 for example.
    AVRational audio_q = av_make_q(1, hz);

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
    render_audio_ctx->sample_rate = audio_q.den;
    render_audio_ctx->ch_layout = channel_layout;
    render_audio_ctx->time_base = audio_q;

    render_audio_stream->time_base = render_audio_ctx->time_base;

    if (render_output_context->oformat->flags & AVFMT_GLOBALHEADER)
    {
        render_audio_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    render_audio_ctx->thread_count = 0; // Use all threads.
    render_audio_ctx->bit_rate = 256 * 1000;

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
        render_audio_ctx->frame_size = 512;
    }

    render_audio_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // Preallocate some frames.

    for (s32 i = 0; i < 8; i++)
    {
        render_get_new_audio_frame();
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

bool EncoderState::render_check_thread_errors()
{
    // Frame thread broke. Nothing more can be submitted.
    if (svr_atom_load(&render_frame_thread_status) == 0)
    {
        error(render_frame_thread_message);
        return true;
    }

    // Packet thread broke. Nothing more can be submitted.
    if (svr_atom_load(&render_packet_thread_status) == 0)
    {
        error(render_packet_thread_message);
        return true;
    }

    return false;
}

// The shared game texture has been updated at this point.
bool EncoderState::render_receive_video()
{
    assert(render_started);

    bool ret = false;

    if (render_check_thread_errors())
    {
        goto rfail;
    }

    AVFrame* frame = render_get_new_video_frame();

    vid_convert_to_codec_textures(frame);

    frame->pts = render_video_pts;

    render_encode_video_frame(frame);

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

    if (render_check_thread_errors())
    {
        goto rfail;
    }

    audio_convert_to_codec_samples();

    // Must submit everything in the fifo so things don't start drifting away.
    // It's possible that this doesn't do anything in case there aren't enough samples to cover the needed frame size.
    render_submit_audio_fifo();

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

// Some encoders need a fixed amount of samples every iteration (except the last).
// We may be getting less samples from the game, so we have to queue the samples up until we have enough to submit.
// Call this when rendering is stopping to submit the slack.
void EncoderState::render_flush_audio_fifo()
{
    s32 num_remaining = audio_num_queued_samples();

    while (num_remaining > 0)
    {
        s32 num_samples = svr_min(num_remaining, render_audio_ctx->frame_size); // Ok for the last submit to have less samples than the frame size.
        render_encode_frame_from_audio_fifo(num_samples);
        num_remaining -= num_samples;
    }
}

// Some encoders need a fixed amount of samples every iteration (except the last).
// We may be getting less samples from the game, so we have to queue the samples up until we have enough to submit.
// Call this during rendering to submit all possible audio frames.
void EncoderState::render_submit_audio_fifo()
{
    s32 num_remaining = audio_num_queued_samples();

    while (num_remaining >= render_audio_ctx->frame_size)
    {
        render_encode_frame_from_audio_fifo(render_audio_ctx->frame_size); // Every submission must have the same size in this case.
        num_remaining -= render_audio_ctx->frame_size;
    }
}

// Common code for render_submit_audio_fifo and render_flush_audio_fifo.
void EncoderState::render_encode_frame_from_audio_fifo(s32 num_samples)
{
    AVFrame* frame = render_get_new_audio_frame();

    audio_copy_samples_to_frame(frame, num_samples);

    frame->pts = render_audio_pts;
    frame->nb_samples = num_samples;
    render_audio_pts += num_samples;

    render_encode_audio_frame(frame);
}

void EncoderState::render_encode_video_frame(AVFrame* frame)
{
    render_encode_frame(render_video_ctx, render_video_stream, frame, AVMEDIA_TYPE_VIDEO);
}

void EncoderState::render_encode_audio_frame(AVFrame* frame)
{
    render_encode_frame(render_audio_ctx, render_audio_stream, frame, AVMEDIA_TYPE_AUDIO);
}

void EncoderState::render_encode_frame(AVCodecContext* ctx, AVStream* stream, AVFrame* frame, AVMediaType type)
{
    // Send to frame thread.

    RenderFrameThreadInput input = {};
    input.ctx = ctx;
    input.frame = frame;
    input.stream = stream;
    input.type = type;

    render_frame_queue.push(&input);

    SetEvent(render_frame_wake_event_h); // Notify frame thread.
}

AVFrame* EncoderState::render_get_new_video_frame()
{
    AVFrame* ret = NULL;
    s32 res;

    // Fast and good if we can reuse.
    if (render_recycled_video_frames.pull(&ret))
    {
        return ret;
    }

    ret = av_frame_alloc();

    if (ret == NULL)
    {
        error("ERROR: Could not create render video encode frame\n");
        goto rfail;
    }

    ret->format = render_video_ctx->pix_fmt;
    ret->width = render_video_ctx->width;
    ret->height = render_video_ctx->height;

    // Allocate buffers for frame.
    res = av_frame_get_buffer(ret, 0);

    if (res < 0)
    {
        error("ERROR: Could not allocate render video encode frame\n");
        goto rfail;
    }

    goto rexit;

rfail:

rexit:
    return ret;
}

AVFrame* EncoderState::render_get_new_audio_frame()
{
    AVFrame* ret = NULL;
    s32 res;

    // Fast and good if we can reuse.
    if (render_recycled_audio_frames.pull(&ret))
    {
        return ret;
    }

    ret = av_frame_alloc();

    if (ret == NULL)
    {
        error("ERROR: Could not create render audio encode frame\n");
        goto rfail;
    }

    ret->format = render_audio_ctx->sample_fmt;
    ret->ch_layout = render_audio_ctx->ch_layout;
    ret->sample_rate = render_audio_ctx->sample_rate;
    ret->nb_samples = render_audio_ctx->frame_size; // All submitted audio frames will have this amount of samples, except the last.

    // Allocate buffers for frame.
    res = av_frame_get_buffer(ret, 0);

    if (res < 0)
    {
        error("ERROR: Could not allocate render audio encode frame\n");
        goto rfail;
    }

    goto rexit;

rfail:

rexit:
    return ret;
}
