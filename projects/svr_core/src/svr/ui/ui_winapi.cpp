#include <svr/ui.hpp>
#include <svr/log_format.hpp>
#include <svr/synchro.hpp>

#include <Windows.h>

enum
{
    // Custom quit message used to signal the message loop completion.
    // WPARAM = nothing
    // LPARAM = address of synchro_barrier that should be opened on completion.
    WM_QUIT_MESSAGE_LOOP = WM_USER + 0,
};

namespace svr
{
    void ui_enter_message_loop()
    {
        MSG msg;

        while (GetMessageA(&msg, nullptr, 0, 0))
        {
            // Thread posted messages cannot be handled by window procedures.
            if (msg.message == WM_QUIT_MESSAGE_LOOP)
            {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        static_assert(sizeof(synchro_barrier*) == sizeof(LPARAM), "size mismatch");

        // Open the barrier to notify that we are done.

        auto barrier = (synchro_barrier*)msg.lParam;
        barrier->open();
    }

    void ui_exit_message_loop(uint64_t id)
    {
        using namespace std::chrono_literals;

        synchro_barrier barrier;

        static_assert(sizeof(synchro_barrier*) == sizeof(LPARAM), "size mismatch");

        // Do stupid polling because messages may get lost when using message thread posting.
        // If a window is in another state then our message loop will get ignored.
        // A window can get in another state by for example moving or resizing it.
        // Just keep trying to send it until our event gets signalled.
        while (true)
        {
            auto res = PostThreadMessageA(id, WM_QUIT_MESSAGE_LOOP, 0, (LPARAM)&barrier);

            if (res == 0)
            {
                log("winapi ui: Could not request message loop in thread {} to exit ({})\n", id, GetLastError());
                break;
            }

            // Wait for the message loop to actually exit.
            if (barrier.wait(100ms))
            {
                break;
            }
        }
    }
}
