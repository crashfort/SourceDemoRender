#pragma once
#include <svr/api.hpp>
#include <svr/synchro.hpp>

#include <functional>
#include <chrono>

#include <concurrentqueue/blockingconcurrentqueue.h>

namespace svr
{
    SVR_API uint64_t os_get_thread_id_self();

    // Thread context which waits indefinitely for new tasks to run.
    class thread_context_event
    {
    public:
        thread_context_event()
        {
            // Block the calling thread until the new thread has written the thread number.
            synchro_barrier barrier;

            thread = std::thread([&]()
            {
                thread_number = os_get_thread_id_self();
                barrier.open();

                while (true)
                {
                    // Wait forever for a new task.
                    std::function<void()> task;
                    invokes.wait_dequeue(task);

                    // Sentinel value - break the iteration.
                    if (task == nullptr)
                    {
                        return;
                    }

                    task();
                }
            });

            barrier.wait();
        }

        ~thread_context_event()
        {
            if (thread.joinable())
            {
                // Push the sentinel value to break the iteration.
                invokes.enqueue(nullptr);
                thread.join();
            }
        }

        // Returns the identifier of the thread that this thread context uses.
        // Can be called from any thread.
        uint64_t get_thread_id() const
        {
            return thread_number;
        }

        // Runs a task asynchronously on this thread context.
        // The task will run synchronously if the calling thread is the same.
        // Optional synchronization parameter can be passed in to wait for completion.
        // Do not send empty tasks.
        // Can be called from any thread.
        void run_task(std::function<void()>&& task, synchro_barrier* barrier = nullptr)
        {
            auto func = [task = std::move(task), barrier]()
            {
                task();

                if (barrier)
                {
                    barrier->open();
                }
            };

            // A task was added while on the same thread.
            if (thread_number == os_get_thread_id_self())
            {
                func();
                return;
            }

            else
            {
                invokes.enqueue(std::move(func));
            }
        }

        // Runs a task on this thread context.
        // Will not return until the function has been called.
        // Do not send empty tasks.
        // Can be called from any thread.
        void run_task_wait(std::function<void()>&& task)
        {
            synchro_barrier barrier;

            run_task(std::move(task), &barrier);

            barrier.wait();
        }

    private:
        std::thread thread;
        uint64_t thread_number;

        moodycamel::BlockingConcurrentQueue<std::function<void()>> invokes;
    };
}
