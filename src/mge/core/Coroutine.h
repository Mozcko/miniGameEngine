#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mge {

    // Tipo de espera devuelto por un paso de corrutina cooperativa (compatible con C++17)
    enum class YieldType {
        Immediate, // Continuar en el mismo update sin esperar
        None,      // Ceder hasta el siguiente update
        Seconds,   // Esperar una duración en segundos
        Frames,    // Esperar N llamadas a update
        Until,     // Esperar hasta que predicate() devuelva true
        WhileTrue, // Esperar mientras predicate() devuelva true
        Done       // Corrutina finalizada
    };

    struct YieldInstruction {
        YieldType type = YieldType::Done;
        float seconds = 0.0f;
        std::uint32_t frames = 0;
        std::function<bool()> predicate;
    };

    // Fábricas legibles para construir instrucciones de espera
    struct Wait {
        static YieldInstruction immediate() { return YieldInstruction{YieldType::Immediate, 0.0f, 0, {}}; }
        static YieldInstruction none() { return YieldInstruction{YieldType::None, 0.0f, 0, {}}; }
        static YieldInstruction seconds(float duration) {
            return YieldInstruction{YieldType::Seconds, duration > 0.0f ? duration : 0.0f, 0, {}};
        }
        static YieldInstruction frames(std::uint32_t count) {
            return YieldInstruction{YieldType::Frames, 0.0f, count, {}};
        }
        static YieldInstruction until(std::function<bool()> predicate) {
            return YieldInstruction{YieldType::Until, 0.0f, 0, std::move(predicate)};
        }
        static YieldInstruction whileTrue(std::function<bool()> predicate) {
            return YieldInstruction{YieldType::WhileTrue, 0.0f, 0, std::move(predicate)};
        }
        static YieldInstruction done() { return YieldInstruction{YieldType::Done, 0.0f, 0, {}}; }
    };

    // Un paso de corrutina devuelve la siguiente instrucción de espera (o Wait::done() al terminar)
    using CoroutineStep = std::function<YieldInstruction()>;

    // Constructor fluido de secuencias de pasos y esperas sin necesidad de escribir máquinas de estado
    class CoroutineSequence {
    public:
        CoroutineSequence& run(std::function<void()> action);
        CoroutineSequence& yieldFrame();
        CoroutineSequence& waitSeconds(float duration);
        CoroutineSequence& waitFrames(std::uint32_t count);
        CoroutineSequence& waitUntil(std::function<bool()> predicate);
        CoroutineSequence& waitWhile(std::function<bool()> predicate);
        CoroutineSequence& step(CoroutineStep customStep);

        CoroutineStep build() &&;
        CoroutineStep build() const&;

    private:
        std::vector<CoroutineStep> m_steps;
    };

    // Handle generacional de una corrutina activa
    struct CoroutineHandle {
        std::uint32_t index = kInvalidIndex;
        std::uint32_t generation = 0;

        static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

        bool isNull() const { return index == kInvalidIndex; }

        friend bool operator==(CoroutineHandle a, CoroutineHandle b) {
            return a.index == b.index && a.generation == b.generation;
        }
        friend bool operator!=(CoroutineHandle a, CoroutineHandle b) { return !(a == b); }
    };

    inline constexpr CoroutineHandle kNullCoroutine{};

    // Ejecutor cooperativo de corrutinas en el hilo principal, avanzado con update(dt).
    // Seguro ante reentrada: los pasos pueden iniciar, pausar o detener otras corrutinas.
    class CoroutineRunner {
    public:
        CoroutineHandle start(CoroutineStep step);
        CoroutineHandle start(CoroutineSequence sequence);

        void update(float dt);

        void stop(CoroutineHandle handle);
        void pause(CoroutineHandle handle);
        void resume(CoroutineHandle handle);

        bool isRunning(CoroutineHandle handle) const;
        bool isPaused(CoroutineHandle handle) const;
        std::size_t runningCount() const { return m_runningCount; }

        void clear();

    private:
        struct Slot {
            std::uint32_t generation = 0;
            bool active = false;
            bool paused = false;
            bool started = false;
            YieldInstruction wait;
            CoroutineStep step;
        };

        void advanceCoroutine(CoroutineHandle handle);
        void releaseSlot(std::uint32_t index);

        std::vector<Slot> m_slots;
        std::vector<std::uint32_t> m_freeIndices;
        std::size_t m_runningCount = 0;
    };
}
