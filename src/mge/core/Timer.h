#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mge {

    // Handle generacional de un temporizador: invalida referencias viejas al reciclar índices
    struct TimerHandle {
        std::uint32_t index = kInvalidIndex;
        std::uint32_t generation = 0;

        static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

        bool isNull() const { return index == kInvalidIndex; }

        friend bool operator==(TimerHandle a, TimerHandle b) {
            return a.index == b.index && a.generation == b.generation;
        }
        friend bool operator!=(TimerHandle a, TimerHandle b) { return !(a == b); }
    };

    inline constexpr TimerHandle kNullTimer{};

    // Gestor de temporizadores de gameplay (one-shot y repetitivos) avanzados con update(dt).
    // Seguro ante reentrada: los callbacks pueden cancelar, pausar o crear nuevos temporizadores.
    class TimerManager {
    public:
        // Dispara callback una sola vez tras delaySeconds (si delaySeconds <= 0, en el próximo update)
        TimerHandle after(float delaySeconds, std::function<void()> callback);

        // Dispara callback cada intervalSeconds (> 0). repeatCount < 0 repite indefinidamente.
        TimerHandle every(float intervalSeconds, std::function<void()> callback, int repeatCount = -1);

        // Avanza todos los temporizadores activos (no pausados) en dt segundos
        void update(float dt);

        void cancel(TimerHandle handle);
        void pause(TimerHandle handle);
        void resume(TimerHandle handle);

        bool isActive(TimerHandle handle) const;
        bool isPaused(TimerHandle handle) const;
        float remaining(TimerHandle handle) const;
        std::size_t activeCount() const { return m_activeCount; }

        void clear();

    private:
        struct Slot {
            std::uint32_t generation = 0;
            bool active = false;
            bool paused = false;
            float remaining = 0.0f;
            float interval = 0.0f;
            int remainingRepeats = 0;
            std::function<void()> callback;
        };

        TimerHandle allocateSlot(float delaySeconds, float intervalSeconds, int repeatCount,
                                 std::function<void()> callback);
        void releaseSlot(std::uint32_t index);

        std::vector<Slot> m_slots;
        std::vector<std::uint32_t> m_freeIndices;
        std::size_t m_activeCount = 0;
    };
}
