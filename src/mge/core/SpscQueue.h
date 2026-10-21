#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mge {

    // Cola sin locks de un productor y un consumidor (#46), de capacidad fija: un hilo hace push y otro
    // pop, sin reservar memoria ni bloquear. La usa el audio (juego → hilo de audio). T se copia tal cual:
    // tiene que ser trivialmente copiable. Capacity: potencia de 2 (entran Capacity - 1 elementos).
    template <typename T, std::size_t Capacity> class SpscQueue {
        static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity debe ser potencia de 2");
        static_assert(std::is_trivially_copyable_v<T>, "SpscQueue copia los elementos byte a byte");

    public:
        // Productor. false = llena (el elemento no entra)
        bool push(const T& item) {
            const std::size_t tail = m_tail.load(std::memory_order_relaxed);
            const std::size_t next = (tail + 1) & kMask;
            if (next == m_head.load(std::memory_order_acquire)) return false;
            m_items[tail] = item;
            m_tail.store(next, std::memory_order_release);
            return true;
        }

        // Consumidor. false = vacía
        bool pop(T& item) {
            const std::size_t head = m_head.load(std::memory_order_relaxed);
            if (head == m_tail.load(std::memory_order_acquire)) return false;
            item = m_items[head];
            m_head.store((head + 1) & kMask, std::memory_order_release);
            return true;
        }

        // Aproximado si el otro hilo está trabajando
        bool empty() const {
            return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }
        static constexpr std::size_t capacity() { return Capacity - 1; }

    private:
        static constexpr std::size_t kMask = Capacity - 1;

        std::array<T, Capacity> m_items{};
        alignas(64) std::atomic<std::size_t> m_head{0}; // Lo avanza el consumidor
        alignas(64) std::atomic<std::size_t> m_tail{0}; // Lo avanza el productor
    };
}
