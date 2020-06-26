#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    // Enters the current thread in a blocking message loop.
    // There can only be one message loop per thread.
    // A message loop will update all windows that are created in that thread.
    SVR_API void ui_enter_message_loop();

    // Requests that a message loop is exited in a thread.
    // All windows that were created in the thread will be closed.
    // Will block until everything is shut down.
    // Can be called from any thread.
    SVR_API void ui_exit_message_loop(uint64_t id);
}
