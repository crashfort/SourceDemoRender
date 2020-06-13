#pragma once
#include <svr/api.hpp>

// Windows specific functions.

namespace svr
{
    struct os_handle;

    // Queues up a function to run in the specified process.
    // This function should only be called when the process is in a suspended state.
    SVR_API bool os_queue_async_thread_func(os_handle* thread, void* func, void* param);
}
