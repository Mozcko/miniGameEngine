#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mge {

    // Lock-free single-producer, single-consumer queue (#46) with fixed capacity: one thread pushes and
    // another pops, with no heap allocation or blocking. Used by audio (game → audio thread). T is copied
    // verbatim: it must be trivially copyable. Capacity: power of 2 (holds Capacity - 1 items).
    template <typename T, std::size_t Capacity> class SpscQueue {
        static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
        static_assert(std::is_trivially_copyable_v<T>, "SpscQueue copies elements byte by byte");

    public:
        // Producer. false = full (item did not fit)
        bool push(const T& item) {
            const std::size_t tail = m_tail.load(std::memory_order_relaxed);
            const std::size_t next = (tail + 1) & kMask;
            if (next == m_head.load(std::memory_order_acquire)) return false;
            m_items[tail] = item;
            m_tail.store(next, std::memory_order_release);
            return true;
        }

        // Consumer. false = empty
        bool pop(T& item) {
            const std::size_t head = m_head.load(std::memory_order_relaxed);
            if (head == m_tail.load(std::memory_order_acquire)) return false;
            item = m_items[head];
            m_head.store((head + 1) & kMask, std::memory_order_release);
            return true;
        }

        // Approximate if the other thread is active
        bool empty() const {
            return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }
        static constexpr std::size_t capacity() { return Capacity - 1; }

    private:
        static constexpr std::size_t kMask = Capacity - 1;

        std::array<T, Capacity> m_items{};
        alignas(64) std::atomic<std::size_t> m_head{0}; // Advanced by the consumer
        alignas(64) std::atomic<std::size_t> m_tail{0}; // Advanced by the producer
    };
}
