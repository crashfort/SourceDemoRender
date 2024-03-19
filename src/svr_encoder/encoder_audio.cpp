#include "encoder_priv.h"

// Conversion from game audio format to audio encoder format.

bool EncoderState::audio_init()
{
    return true;
}

void EncoderState::audio_free_static()
{
}

void EncoderState::audio_free_dynamic()
{
    if (audio_output_buffers[0])
    {
        av_freep(&audio_output_buffers[0]);
    }

    swr_free(&audio_swr);

    if (audio_fifo)
    {
        av_audio_fifo_free(audio_fifo);
        audio_fifo = NULL;
    }
}

bool EncoderState::audio_start()
{
    bool ret = false;

    if (!audio_create_resampler())
    {
        goto rfail;
    }

    if (!audio_create_fifo())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::audio_create_resampler()
{
    bool ret = false;
    s32 res;

    audio_input_hz = movie_params.audio_hz;
    audio_output_hz = movie_params.audio_hz;

    // Set from encoder if it requires a set sample rate.
    if (render_audio_info->hz != 0)
    {
        audio_output_hz = render_audio_info->hz;
    }

    audio_num_channels = movie_params.audio_channels;

    AVChannelLayout channel_layout;
    av_channel_layout_default(&channel_layout, movie_params.audio_channels);

    AVSampleFormat input_format; // Sample format we get from svr_game.

    switch (movie_params.audio_bits)
    {
        case 16:
        {
            input_format = AV_SAMPLE_FMT_S16; // Interleaved samples.
            break;
        }

        default:
        {
            error("ERROR: Number of bits for audio not supported: %d\n", movie_params.audio_bits);
            goto rfail;
        }
    }

    // If the input is matching the output, we can just copy directly over and we don't need to do any of this. Nice!

    if (input_format == render_audio_info->sample_format)
    {
        if (audio_input_hz == audio_output_hz)
        {
            ret = true;
            goto rexit;
        }
    }

    audio_output_size = 0;

    for (s32 i = 0; i < AUDIO_MAX_CHANS; i++)
    {
        audio_output_buffers[i] = NULL;
    }

    res = swr_alloc_set_opts2(&audio_swr, &channel_layout, render_audio_info->sample_format, audio_output_hz, &channel_layout, input_format, audio_input_hz, 0, NULL);

    if (res < 0)
    {
        error("ERROR: Could not create audio resampler (%d)\n", res);
        goto rfail;
    }

    res = swr_init(audio_swr);

    if (res < 0)
    {
        error("ERROR: Could not initialize audio resampler (%d)\n", res);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    av_channel_layout_uninit(&channel_layout);
    return ret;
}

bool EncoderState::audio_create_fifo()
{
    bool ret = false;

    audio_fifo = av_audio_fifo_alloc(render_audio_info->sample_format, audio_num_channels, render_audio_ctx->frame_size * 2);

    if (audio_fifo == NULL)
    {
        error("ERROR: Could not create audio fifo");
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

// Copies the audio samples into the fifo.
// The samples get converted if needed.
void EncoderState::audio_convert_to_codec_samples(RenderAudioThreadInput* buffer)
{
    // Try to avoid extra procesing if we can.
    // If we have matching input and output parameters, just copy over and return.

    if (audio_need_conversion())
    {
        // The delay are queued samples that are needed when resampling, because the resampling algorithm
        // requires future samples for interpolation. This may be 16 samples for example, that will be used to interpolate
        // with future samples we give it. Those samples are accounted for here, so they will not be forgotten about.
        // This is why you cannot get exact amount of samples back that you give in, as then you would notice the harsh
        // change in the curve where the interpolation breaks. If no resampling is needed (if the sample rate matches what we want)
        // then there will not be any queued samples.
        // We must always fill at least one paint buffer. If we end up getting more samples than anticipated, then those will be stored in the channel.
        s64 delay = swr_get_delay(audio_swr, audio_input_hz);

        // Don't use more than necessary here, because we should not be receiving tons of more samples than we need.
        s64 estimated = av_rescale_rnd(delay + (int64_t)buffer->num_samples, audio_output_hz, audio_input_hz, AV_ROUND_UP);

        // Grow buffer.
        if (estimated > audio_output_size)
        {
            if (audio_output_buffers[0])
            {
                av_freep(&audio_output_buffers[0]);
            }

            av_samples_alloc(audio_output_buffers, NULL, audio_num_channels, estimated, render_audio_info->sample_format, 0);

            audio_output_size = estimated;
        }

        // For the first call, we may get less samples than written because during resampling, additional samples
        // need to be kept for the interpolation. We will call again with the number of samples we are missing to exactly fill out one paint buffer.
        s32 num_output_samples = swr_convert(audio_swr, audio_output_buffers, audio_output_size, (const uint8_t**)&buffer->mem, buffer->num_samples);

        // Some encoders have a restriction that they only work with a fixed amount of samples.
        // We can get less samples from the game so we have to queue them up and only copy to a frame when we have enough.
        av_audio_fifo_write(audio_fifo, (void**)audio_output_buffers, num_output_samples);
    }

    // Matching parameters, just copy over.
    else
    {
        // Some encoders have a restriction that they only work with a fixed amount of samples.
        // We can get less samples from the game so we have to queue them up and only copy to a frame when we have enough.
        av_audio_fifo_write(audio_fifo, (void**)&buffer->mem, buffer->num_samples);
    }
}

void EncoderState::audio_copy_samples_to_frame(AVFrame* dest_frame, s32 num_samples)
{
    assert(num_samples <= audio_num_queued_samples());
    av_audio_fifo_read(audio_fifo, (void**)dest_frame->data, num_samples);
}

s32 EncoderState::audio_num_queued_samples()
{
    s32 num_samples = av_audio_fifo_size(audio_fifo);
    return num_samples;
}

bool EncoderState::audio_need_conversion()
{
    return audio_swr;
}
