#pragma once
#include "svr_common.h"
#include "svr_atom.h"

// Shared stuff between svr_game, svr_standalone and svr_studio.

using StudioSharedCmdId = s32;

enum // StudioSharedCmdId
{
    STUDIO_SHARED_CMD_NONE,

    // Returns after demo is started.
    STUDIO_SHARED_CMD_PLAY_DEMO,

    // Returns after replay is started.
    STUDIO_SHARED_CMD_PLAY_REPLAY,

    // Returns after the demo skipping is complete.
    STUDIO_SHARED_CMD_SKIP_TO_START,

    // Returns after movie is ended.
    STUDIO_SHARED_CMD_START_REC,
};

// STUDIO_SHARED_CMD_PLAY_DEMO.
struct StudioSharedPlayDemoCmd
{
    char demo_name[256];
};

// STUDIO_SHARED_CMD_PLAY_REPLAY.
struct StudioSharedPlayReplayCmd
{
    char map_name[128];
    char replay_name[256];
    char replay_system_dir[256];
};

// STUDIO_SHARED_CMD_SKIP_TO_START.
struct StudioSharedSkipToStartCmd
{
    s32 tick;
};

// STUDIO_SHARED_CMD_START_REC.
struct StudioSharedStartRecCmd
{
    // Steam ID used when joining the server.
    // This is required because it is not guaranteed spectating by name will work, as the command fails if there are several matches.
    char steam_id[128];

    // Movie profile to use.
    char profile[64];

    // Name of movie with extension, no path.
    char movie_name[256];

    // Length of movie in seconds.
    s32 movie_length;
};

union StudioSharedCommandData
{
    StudioSharedPlayDemoCmd play_demo_cmd;
    StudioSharedPlayReplayCmd play_replay_cmd;
    StudioSharedSkipToStartCmd skip_to_start_cmd;
    StudioSharedStartRecCmd start_rec_cmd;
};

struct StudioSharedPeer
{
    SVR_THREAD_PADDING();

    // Will be set to a value other than STUDIO_SHARED_CMD_NONE when an event is pending.
    SvrAtom32 pending_cmd;

    SVR_THREAD_PADDING();

    // Should be set after handling an event.
    u32 wake_studio_h;

    // Command specific data, input and output.
    StudioSharedCommandData cmd_data;

    // Write error strings to this.
    char error[256];

    SVR_THREAD_PADDING();
};

// Memory that is shared between the processes.
// Any command interaction with a peer is initiated by svr_studio.
// The events listed in StudioSharedCmdId above are blocking.
// The studio process will fill and send an event to a peer, set pending_cmd, and then wait for its studio wake event for the game process to respond.
// Both queries and commands are implemented in this way.
struct StudioSharedMem
{
    SVR_THREAD_PADDING();

    StudioSharedPeer game_peer;

    SVR_THREAD_PADDING();

    StudioSharedPeer proc_peer;

    SVR_THREAD_PADDING();

    // Video time svr_game is on, in milliseconds.
    // The length is already known by the recording timeout.
    // Written by svr_game and read by svr_studio.
    SvrAtom32 video_time;

    SVR_THREAD_PADDING();

    // Cancel the rendering process.
    // Written by svr_studio and read by svr_standalone.
    SvrAtom32 cancel;

    SVR_THREAD_PADDING();

    // Studio process id.
    u32 studio_pid;

    // Unique identifier for this studio session (the rendering command id).
    s32 session;

    SVR_THREAD_PADDING();
};
