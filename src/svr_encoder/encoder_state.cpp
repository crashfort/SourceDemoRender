#include "encoder_priv.h"

bool EncoderState::init(HANDLE in_shared_mem_h)
{
    bool ret = false;

    main_thread_id = GetCurrentThreadId();

    shared_mem_h = in_shared_mem_h;

    // At this point, the shared memory will already have some data already filled in.
    shared_mem_ptr = (EncoderSharedMem*)MapViewOfFile(shared_mem_h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

    if (shared_mem_ptr == NULL)
    {
        error("ERROR: Could not view encoder shared memory (%lu)\n", GetLastError());
        goto rfail;
    }

    game_wake_event_h = (HANDLE)shared_mem_ptr->game_wake_event_h;
    encoder_wake_event_h = (HANDLE)shared_mem_ptr->encoder_wake_event_h;
    shared_audio_buffer = (u8*)shared_mem_ptr + shared_mem_ptr->audio_buffer_offset;

    game_process = OpenProcess(SYNCHRONIZE, FALSE, shared_mem_ptr->game_pid);

    if (game_process == NULL)
    {
        error("ERROR: Could not open game process (%lu)\n", GetLastError());
        goto rfail;
    }

    if (!vid_init())
    {
        goto rfail;
    }

    if (!audio_init())
    {
        goto rfail;
    }

    if (!render_init())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    free_static();

rexit:
    return ret;
}

void EncoderState::start_event()
{
    svr_log("Starting encoder\n");

    // The movie parameters in the shared memory won't change after this point, but we
    // want to have our own copy either way.
    movie_params = shared_mem_ptr->movie_params;

    if (!render_start())
    {
        goto rfail;
    }

    if (!vid_start())
    {
        goto rfail;
    }

    if (movie_params.use_audio)
    {
        if (!audio_start())
        {
            goto rfail;
        }
    }

    if (render_video_info)
    {
        svr_log("Using video encoder %s\n", render_video_info->profile_name);
    }

    if (render_audio_info)
    {
        svr_log("Using audio encoder %s\n", render_audio_info->profile_name);
    }

    goto rexit;

rfail:
    stop_after_dynamic_error();

rexit:
    return;
}

void EncoderState::stop_event()
{
    svr_log("Ending encoder\n");

    free_dynamic();
}

void EncoderState::new_video_frame_event()
{
    if (!render_receive_video())
    {
        stop_after_dynamic_error();
    }
}

void EncoderState::new_audio_samples_event()
{
    if (!render_receive_audio())
    {
        stop_after_dynamic_error();
    }
}

// Event reading from svr_game.
void EncoderState::event_loop()
{
    svr_log("Encoder ready\n");

    HANDLE handles[] = {
        game_process,
        encoder_wake_event_h,
    };

    while (true)
    {
        DWORD waited = WaitForMultipleObjects(SVR_ARRAY_SIZE(handles), handles, FALSE, INFINITE);
        HANDLE waited_h = handles[waited - WAIT_OBJECT_0];

        // Game exited or crashed or something.
        // If we were recording, we did not get the stop command, so just stop as if we got it.
        // This will exit this process too.
        if (waited_h == game_process)
        {
            if (render_started)
            {
                svr_log("Game exited without telling the encoder, ending movie\n");

                stop_event();
            }

            break;
        }

        // We are woken up here because svr_game wants us to do something.
        // Any code in here needs to be fast because the game is frozen at this point.
        // Forward relevant stuff to the actual encoder thread.
        if (waited_h == encoder_wake_event_h)
        {
            // Clear out any error from previous calls.
            shared_mem_ptr->error = 0;
            shared_mem_ptr->error_message[0] = 0;

            switch (shared_mem_ptr->event_type)
            {
                case ENCODER_EVENT_START:
                {
                    start_event();
                    break;
                }

                case ENCODER_EVENT_STOP:
                {
                    stop_event();
                    break;
                }

                case ENCODER_EVENT_NEW_VIDEO:
                {
                    new_video_frame_event();
                    break;
                }

                case ENCODER_EVENT_NEW_AUDIO:
                {
                    new_audio_samples_event();
                    break;
                }
            }

            // Notify svr_game that we handled this event.
            // We go back to sleep after this, which puts us in a known paused state.
            SetEvent(game_wake_event_h);
        }
    }

    svr_log("Encoder finished\n");
}

void EncoderState::free_static()
{
    if (game_process)
    {
        CloseHandle(game_process);
        game_process = NULL;
    }

    if (shared_mem_h)
    {
        CloseHandle(shared_mem_h);
        shared_mem_h = NULL;
    }

    if (shared_mem_ptr)
    {
        UnmapViewOfFile(shared_mem_ptr);
        shared_mem_ptr = NULL;
        shared_audio_buffer = NULL;
    }

    if (game_wake_event_h)
    {
        CloseHandle(game_wake_event_h);
        game_wake_event_h = NULL;
    }

    if (encoder_wake_event_h)
    {
        CloseHandle(encoder_wake_event_h);
        encoder_wake_event_h = NULL;
    }

    render_free_static();
    vid_free_static();
    audio_free_static();
}

void EncoderState::free_dynamic()
{
    render_free_dynamic();
    vid_free_dynamic();
    audio_free_dynamic();
}

void EncoderState::error(const char* format, ...)
{
    // Must only be called by the main thread because the shared memory can only be written by the main thread.
    assert(GetCurrentThreadId() == main_thread_id);

    // Set this early so we don't try to flush the encoders or something.
    // If we have an error then we must stop right now, and not try to process any more data.
    render_started = false;

    char buf[1024];

    va_list va;
    va_start(va, format);

    s32 count = SVR_VSNPRINTF(buf, format, va);

    svr_copy_string(buf, shared_mem_ptr->error_message, SVR_ARRAY_SIZE(shared_mem_ptr->error_message));

    va_end(va);
}

void EncoderState::stop_after_dynamic_error()
{
    // Set error which svr_game will read after we return.
    // Error message has been written to the shared memory through the error function.
    shared_mem_ptr->error = 1;

    free_dynamic();
}
