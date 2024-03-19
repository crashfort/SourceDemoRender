#include "encoder_priv.h"

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

DWORD CALLBACK render_audio_thread_proc(LPVOID param)
{
    SetThreadDescription(GetCurrentThread(), L"RENDER AUDIO THREAD");

    EncoderState* encoder_ptr = (EncoderState*)param;
    encoder_ptr->render_audio_proc();

    return 0; // Not used.
}

bool EncoderState::render_start_threads()
{
    render_frame_thread_h = CreateThread(NULL, 0, render_frame_thread_proc, this, 0, NULL);
    render_packet_thread_h = CreateThread(NULL, 0, render_packet_thread_proc, this, 0, NULL);
    render_audio_thread_h = CreateThread(NULL, 0, render_audio_thread_proc, this, 0, NULL);

    return true;
}

// In frame thread.
void EncoderState::render_frame_proc()
{
    bool run = true;

    while (run)
    {
        WaitForSingleObject(render_frame_wake_event_h, INFINITE);

        // Exit thread on external error.
        if (svr_atom_load(&render_started) == 0)
        {
            break;
        }

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

        // Exit thread on external error.
        if (svr_atom_load(&render_started) == 0)
        {
            break;
        }

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

// In audio thread.
void EncoderState::render_audio_proc()
{
    bool run = true;

    while (run)
    {
        WaitForSingleObject(render_audio_wake_event_h, INFINITE);

        // Exit thread on external error.
        if (svr_atom_load(&render_started) == 0)
        {
            break;
        }

        RenderAudioThreadInput buffer = {};

        while (render_audio_queue.pull(&buffer))
        {
            if (buffer.mem == NULL)
            {
                run = false; // Stop on flush buffer.
            }

            audio_convert_to_codec_samples(&buffer);

            // Must submit everything in the fifo so things don't start drifting away.
            // It's possible that this doesn't do anything in case there aren't enough samples to cover the needed frame size.
            render_submit_audio_fifo();

            render_recycled_audio_buffers.push(&buffer); // Give back the audio buffer.
        }
    }

    goto rexit;

rfail:
    svr_atom_store(&render_audio_thread_status, 0);

rexit:
    return;
}
