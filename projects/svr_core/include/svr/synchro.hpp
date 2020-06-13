#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace svr
{
    // Barrier that can be used for thread synchronization.
    // The barrier is by default closed.
    class synchro_barrier
    {
    public:
        // Opens the barrier, and all waiters will be notified.
        // If the barrier is already open, nothing will happen.
        void open()
        {
            set(true);
        }

        // Closes the barrier, will only call the callback.
        // If the barrier is already closed, nothing will happen.
        void close()
        {
            set(false);
        }

        // Calls either open or close.
        // In the case of opening, all waiters will be notified.
        // If the barrier is already in the same state, nothing will happen.
        void set(bool value)
        {
            std::lock_guard lock(mutex);

            if (signalled == value)
            {
                return;
            }

            signalled = value;

            cv.notify_all();
        }

        // Checks if the barrier is open or closed without waiting.
        bool is_open() const
        {
            std::lock_guard lock(mutex);
            return signalled;
        }

        // Waits for the barrier to be opened.
        // Optional time can be supplied for the maximum time in ms to wait, or indefinitely.
        // Returns false if timeout.
        bool wait(std::chrono::milliseconds timeout = {}) const
        {
            auto condition = [=]()
            {
                return signalled != false;
            };

            if (timeout.count() > 0)
            {
                std::unique_lock lock(mutex);
                return cv.wait_for(lock, timeout, condition);
            }

            else
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, condition);
            }

            return true;
        }

    private:
        bool signalled = false;

        mutable std::condition_variable cv;
        mutable std::mutex mutex;
    };
}
