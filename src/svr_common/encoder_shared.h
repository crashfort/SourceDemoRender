#pragma once
#include "svr_common.h"

// Shared stuff between 32-bit svr_game and 64-bit svr_encoder.

// All Windows handles only use 32 bits of data, so we can safely refer to them in here as u32 with _h in the name.
// https://learn.microsoft.com/en-us/windows/win32/winprog64/interprocess-communication

const s32 ENCODER_MAX_SAMPLES = 4096; // How many samples can be stored at most in the buffer placed at audio_buffer_offset.

// Identifiers used by the DXGI lock for synchronizing with the shared texture.
// You need to specify which device to give access to, so that's what these are.
const s32 ENCODER_GAME_ID = 0;
const s32 ENCODER_PROC_ID = 1;

using EncoderSharedEvent = s32;

enum /* EncoderSharedEvent */
{
    ENCODER_EVENT_NONE,
    ENCODER_EVENT_START, // Movie parameters will be setup. This event can fail.
    ENCODER_EVENT_STOP, // Rendering will stop. This event cannot fail.
    ENCODER_EVENT_NEW_VIDEO, // Texture at game_texture_h will have new data. This event can fail.
    ENCODER_EVENT_NEW_AUDIO, // New samples will be placed at audio_buffer_offset. This event can fail.
};

struct EncoderSharedMovieParams
{
    char dest_file[256];

    // Incoming data specs:
    s32 video_height;
    s32 video_width;
    s32 audio_channels;
    s32 audio_hz;
    s32 audio_bits;

    // Output data specs:
    // These are verified by svr_game already, so svr_encoder can read from them safely.
    char video_encoder[32];
    char audio_encoder[32];
    char x264_preset[32];
    char dnxhr_profile[32];
    s32 video_fps;
    s32 x264_crf;
    bool x264_intra;
    bool use_audio;
};

// Memory that is shared between the processes.
struct EncoderSharedMem
{
    EncoderSharedMovieParams movie_params; // Movie parameters and profile stuff set by svr_game on ENCODER_EVENT_START.

    // Shared handle to the latest game texture in the B8G8R8A8 format. Updated on ENCODER_EVENT_NEW_VIDEO.
    u32 game_texture_h;

    // Pointer types have different sizes in 32-bit and 64-bit so we have to store the offsets from the base
    // of the shared memory instead. The audio samples here are updated on ENCODER_EVENT_NEW_AUDIO.
    s32 audio_buffer_offset;

    s32 waiting_audio_samples; // Set by svr_game to how many audio samples are waiting at audio_buffer_offset. Updated on ENCODER_EVENT_NEW_AUDIO.

    u32 game_wake_event_h; // Event set by svr_encoder to wake svr_game up.
    u32 encoder_wake_event_h; // Event set by svr_game to wake svr_encoder up.
    u32 game_pid; // Game process id. Used by svr_encoder to know if the game exits so we don't get stuck.

    EncoderSharedEvent event_type; // Set by svr_game to let svr_encoder know what to do when woken up. Updated on all events.

    s32 error; // Set to 1 by svr_encoder on any error. A message will be written to error_message.
    char error_message[512]; // Any encoding error will be written here by svr_encoder when error is set to 1.
};
