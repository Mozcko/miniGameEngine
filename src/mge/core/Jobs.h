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

    // Pool de hilos simple: una cola compartida (mutex + condition variable) y N hilos que toman trabajos.
    // Pensado para tareas medianas e independientes (generar/mallar un chunk): sin robo de trabajo ni
    // prioridades. Los trabajos no deben tocar estado del hilo principal sin sincronizar.
    class JobSystem {
    public:
        // threads = 0 → hardware_concurrency − 1 (al menos 1): el hilo principal queda para el juego
        explicit JobSystem(unsigned threads = 0);
        // Termina los trabajos en curso, descarta los que no empezaron y une los hilos
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        // Encola fn y devuelve su resultado como future (las excepciones también viajan en el future).
        // Un future de un trabajo descartado al destruir el pool lanza std::future_error (broken_promise).
        template <typename F> auto submit(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
            using Result = std::invoke_result_t<std::decay_t<F>>;
            auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(fn));
            std::future<Result> future = task->get_future();
            enqueue([task] { (*task)(); });
            return future;
        }

        // Bloquea hasta que no quede ningún trabajo encolado ni en curso
        void waitIdle();

        unsigned threadCount() const { return static_cast<unsigned>(m_threads.size()); }
        // Trabajos encolados + en curso
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
