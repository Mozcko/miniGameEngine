#include <mge/core/Jobs.h>

#include <algorithm>

namespace mge {

    JobSystem::JobSystem(unsigned threads) {
        if (threads == 0) {
            const unsigned hardware = std::thread::hardware_concurrency();
            threads = std::max(1u, hardware > 1 ? hardware - 1 : 1u);
        }
        m_threads.reserve(threads);
        for (unsigned i = 0; i < threads; i++) m_threads.emplace_back([this] { workerLoop(); });
    }

    JobSystem::~JobSystem() {
        std::deque<std::function<void()>> dropped;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
            dropped.swap(m_queue); // Sin ejecutar: sus futures quedan en broken_promise
        }
        m_workAvailable.notify_all();
        for (std::thread& thread : m_threads) thread.join();
        // `dropped` se destruye aquí, fuera del lock (destruye las packaged_task)
    }

    void JobSystem::enqueue(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(job));
        }
        m_workAvailable.notify_one();
    }

    void JobSystem::workerLoop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_workAvailable.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
                if (m_stopping) return;
                job = std::move(m_queue.front());
                m_queue.pop_front();
                m_running++;
            }
            job(); // packaged_task: las excepciones quedan en el future
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_running--;
                if (m_running == 0 && m_queue.empty()) m_idle.notify_all();
            }
        }
    }

    void JobSystem::waitIdle() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle.wait(lock, [this] { return m_queue.empty() && m_running == 0; });
    }

    std::size_t JobSystem::pendingCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size() + m_running;
    }
}
