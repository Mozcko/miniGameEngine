#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace mge {

    // Simple thread pool: one shared queue (mutex + condition variable) and N worker threads.
    // Designed for medium-sized, independent tasks (generating/meshing a chunk): no work stealing or
    // priorities. Jobs must not touch main-thread state without synchronization.
    class JobSystem {
    public:
        // threads = 0 → hardware_concurrency − 1 (at least 1): keeps the main thread free for the game
        explicit JobSystem(unsigned threads = 0);
        // Finishes running jobs, discards queued ones that haven't started, and joins all threads
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        // Enqueues fn and returns its result as a future (exceptions also travel through the future).
        // A future for a job discarded on pool destruction throws std::future_error (broken_promise).
        template <typename F> auto submit(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
            using Result = std::invoke_result_t<std::decay_t<F>>;
            auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(fn));
            std::future<Result> future = task->get_future();
            enqueue([task] { (*task)(); });
            return future;
        }

        // Blocks until no jobs remain queued or running
        void waitIdle();

        unsigned threadCount() const { return static_cast<unsigned>(m_threads.size()); }
        // Queued + running jobs
        std::size_t pendingCount() const;

    private:
        void enqueue(std::function<void()> job);
        void workerLoop();

        std::vector<std::thread> m_threads;
        std::deque<std::function<void()>> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_workAvailable;
        std::condition_variable m_idle;
        std::size_t m_running = 0;
        bool m_stopping = false;
    };
}
