#include "svr_movproc.h"
#include "svr_logging.h"
#include "svr_mem_range.h"
#include "svr_stream.h"
#include <Windows.h>
#include <assert.h>

struct SvrMovProcState
{
    SvrAsyncStream<u8*> frame_read_queue;
    SvrAsyncStream<u8*> frame_write_queue;
    s64 frame_number;
    u8* frame_buf;
    HANDLE write_sem;
    HANDLE read_sem;
};

HANDLE movproc_mmap;
void* movproc_view;

SvrMovProcState* movproc_state;

const char* MOVPROC_MMAP_NAME = "crashfort.svr.movproc_mmap";

void svr_create_movproc(s64 one_frame_size)
{
    LARGE_INTEGER large;
    large.QuadPart = 1 * 1024 * 1024 * 1024;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    movproc_mmap = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, large.HighPart, large.LowPart, MOVPROC_MMAP_NAME);
    movproc_view = MapViewOfFile(movproc_mmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    SvrMemoryRange range;
    range.start = movproc_view;
    range.used = 0;
    range.size = large.QuadPart;

    movproc_state = (SvrMovProcState*)range.push(sizeof(SvrMovProcState));
    movproc_state->frame_read_queue.init_with_range(range, SVR_MOVPROC_QUEUED_FRAMES);
    movproc_state->frame_write_queue.init_with_range(range, SVR_MOVPROC_QUEUED_FRAMES);

    movproc_state->frame_buf = (u8*)range.push(sizeof(u8) * (one_frame_size * SVR_MOVPROC_QUEUED_FRAMES));

    movproc_state->write_sem = CreateSemaphoreA(NULL, 0, SVR_MOVPROC_QUEUED_FRAMES, NULL);
    movproc_state->read_sem = CreateSemaphoreA(NULL, SVR_MOVPROC_QUEUED_FRAMES - 1, SVR_MOVPROC_QUEUED_FRAMES, NULL);

    movproc_state->frame_number = 0;
}

void svr_close_movproc()
{
    UnmapViewOfFile(movproc_view);
    CloseHandle(movproc_mmap);
}

void svr_open_movproc()
{
    movproc_mmap = OpenFileMappingA(GENERIC_WRITE | GENERIC_READ, FALSE, MOVPROC_MMAP_NAME);
    movproc_view = MapViewOfFile(movproc_mmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    movproc_state = (SvrMovProcState*)movproc_view;
}

u8* svr_get_free_movproc_buf()
{
    // In case we have used up all of the available buffered textures, we will have to wait.
    WaitForSingleObject(movproc_state->read_sem, INFINITE);

    u8* mem;
    bool res1 = movproc_state->frame_read_queue.pull(&mem);
    assert(res1);

    return mem;
}

void svr_push_movproc_buf(u8* mem)
{
    bool res1 = movproc_state->frame_write_queue.push(mem);
    assert(res1);

    ReleaseSemaphore(movproc_state->write_sem, 1, NULL);
}

u8* svr_get_used_movproc_buf()
{
    WaitForSingleObject(movproc_state->write_sem, INFINITE);

    u8* mem;
    bool res1 = movproc_state->frame_write_queue.pull(&mem);
    assert(res1);

    return mem;
}

void svr_pull_movproc_buf(u8* mem)
{
    bool res1 = movproc_state->frame_read_queue.push(mem);
    assert(res1);

    ReleaseSemaphore(movproc_state->read_sem, 1, NULL);
}
