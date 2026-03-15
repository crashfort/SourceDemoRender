#include "proc_priv.h"
#include "proc_state.h"

bool ProcState::studio_init()
{
    bool ret = false;

    const char* studio_arg_pos = strstr(GetCommandLineA(), "--studio_h");

    if (studio_arg_pos == NULL)
    {
        // No studio used.
        ret = true;
        goto rexit;
    }

    u32 studio_h = 0;
    sscanf(studio_arg_pos, "--studio_h %u", &studio_h);

    if (studio_h == 0)
    {
        // Just ignore.
        ret = true;
        goto rexit;
    }

    // Not sure how to verify this parameter.
    // We inherit handles when creating this process, so we can just read the handle address directly.
    // All handles only have 32 bits significant, so this is safe in both 32 and 64 bit.
    studio_shared_mem_h = (HANDLE)studio_h;

    // At this point, the shared memory will already have some data already filled in.
    studio_shared_ptr = (StudioSharedMem*)MapViewOfFile(studio_shared_mem_h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

    if (studio_shared_ptr == NULL)
    {
        DWORD error = GetLastError();

        svr_log("ERROR: Could not view studio shared memory (%lu). Full command line: %s\n", error, GetCommandLineA());
        goto rfail;
    }

    studio_proc_h = OpenProcess(SYNCHRONIZE, FALSE, studio_shared_ptr->studio_pid);

    if (studio_proc_h == NULL)
    {
        DWORD error = GetLastError();

        svr_log("ERROR: Could not open game process (%lu)\n", error);
        goto rfail;
    }

    studio_peer = &studio_shared_ptr->proc_peer;

    svr_log("Proc (Studio): Connected to SVR Studio\n");

    // Notify started.
    SetEvent((HANDLE)studio_peer->wake_studio_h);

    ret = true;
    goto rexit;

rfail:
rexit:
    return ret;
}

void ProcState::studio_free_static()
{
    if (studio_proc_h)
    {
        CloseHandle(studio_proc_h);
        studio_proc_h = NULL;
    }

    if (studio_shared_mem_h)
    {
        CloseHandle(studio_shared_mem_h);
        studio_shared_mem_h = NULL;
    }

    if (studio_shared_ptr)
    {
        UnmapViewOfFile(studio_shared_ptr);
        studio_shared_ptr = NULL;
    }
}

void ProcState::studio_free_dynamic()
{
}

bool ProcState::studio_start()
{
    if (!studio_active())
    {
        return true;
    }

    svr_atom_store(&studio_shared_ptr->video_time, 0);
    return true;
}

bool ProcState::studio_active()
{
    return studio_shared_mem_h;
}

void ProcState::studio_update()
{
    if (!studio_active())
    {
        return;
    }

    studio_update_progress();

    s32 cmd_id = svr_atom_swap(&studio_peer->pending_cmd, STUDIO_SHARED_CMD_NONE);

    if (cmd_id == STUDIO_SHARED_CMD_NONE)
    {
        // Nothing new.
        return;
    }

    svr_log("Proc (Studio): Received command: %d\n", cmd_id);

    switch (cmd_id)
    {
        default:
        {
            svr_copy_string(svr_va("Proc (Studio): Unknown command: %d", cmd_id), studio_peer->error, SVR_ARRAY_SIZE(StudioSharedPeer::error));
            SetEvent((HANDLE)studio_peer->wake_studio_h);
            break;
        }
    }

rfail:
rexit:
    ;
}

void ProcState::studio_update_progress()
{
    // Studio wants time as milliseconds.

    double video_time = (double)encoder_sent_video_frames / (double)movie_profile.video_fps;
    s32 stored_video_time = video_time * 1000.0;

    svr_atom_store(&studio_shared_ptr->video_time, stored_video_time);
}
