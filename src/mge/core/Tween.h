#pragma once

#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace mge {

    // Curvas de suavizado estándar (Robert Penner) para interpolación en [0, 1]
    enum class Ease {
        Linear,
        InQuad,
        OutQuad,
        InOutQuad,
        InCubic,
        OutCubic,
        InOutCubic,
        InSine,
        OutSine,
        InOutSine,
        InExpo,
        OutExpo,
        InOutExpo,
        InBack,
        OutBack,
        InOutBack,
        InBounce,
        OutBounce,
        InOutBounce,
        InElastic,
        OutElastic,
        InOutElastic
    };

    // Evalúa una curva de suavizado con t recortado a [0, 1].
    // Garantiza applyEase(e, 0) == 0 y applyEase(e, 1) == 1 para todas las curvas.
    float applyEase(Ease ease, float t);

    // Interpolación lineal genérica (float, double, glm::vec2/3/4, etc.)
    template <typename T> T lerpValue(const T& from, const T& to, float t) {
        return from + (to - from) * t;
    }

    // Sobrecarga esférica para cuaterniones
    inline glm::quat lerpValue(const glm::quat& from, const glm::quat& to, float t) {
        return glm::slerp(from, to, t);
    }

    enum class LoopMode {
        Once,     // Se ejecuta una sola vez y termina
        Restart,  // Vuelve a 0 al completar cada ciclo
        PingPong  // Alterna entre 0 -> 1 y 1 -> 0 en cada tramo
    };

    // Handle generacional de un tween
    struct TweenHandle {
        std::uint32_t index = kInvalidIndex;
        std::uint32_t generation = 0;

        static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

        bool isNull() const { return index == kInvalidIndex; }

        friend bool operator==(TweenHandle a, TweenHandle b) {
            return a.index == b.index && a.generation == b.generation;
        }
        friend bool operator!=(TweenHandle a, TweenHandle b) { return !(a == b); }
    };

    inline constexpr TweenHandle kNullTween{};

    // Gestor de tweens avanzados con update(dt). Soporta easing estándar o personalizado,
    // retraso inicial, bucles (Once/Restart/PingPong), pausa y callback de finalización.
    class TweenManager {
    public:
        // Crea un tween genérico: onProgress recibe el progreso ya suavizado
        TweenHandle add(float durationSeconds, std::function<void(float)> onProgress,
                        Ease ease = Ease::Linear);

        // Interpola un valor entre from y toValue llamando a setter en cada actualización
        template <typename T>
        TweenHandle value(const T& from, const T& toValue, float durationSeconds,
                          std::function<void(const T&)> setter, Ease ease = Ease::Linear) {
            if (!setter) return kNullTween;
            return add(
                durationSeconds,
                [from, toValue, fn = std::move(setter)](float easedT) {
                    fn(lerpValue(from, toValue, easedT));
                },
                ease);
        }

        // Atajo que escribe directamente en target (debe vivir mientras el tween esté activo)
        template <typename T>
        TweenHandle to(T& target, const T& from, const T& toValue, float durationSeconds,
                       Ease ease = Ease::Linear) {
            return add(
                durationSeconds,
                [&target, from, toValue](float easedT) { target = lerpValue(from, toValue, easedT); },
                ease);
        }

        void setDelay(TweenHandle handle, float delaySeconds);
        // loopCount < 0 repite indefinidamente; loopCount == 0 se trata como 1
        void setLoop(TweenHandle handle, LoopMode mode, int loopCount = -1);
        void setEase(TweenHandle handle, Ease ease);
        void setEasingFunction(TweenHandle handle, std::function<float(float)> customEase);
        void setOnComplete(TweenHandle handle, std::function<void()> callback);

        void update(float dt);

        void cancel(TweenHandle handle);
        void pause(TweenHandle handle);
        void resume(TweenHandle handle);

        bool isActive(TweenHandle handle) const;
        bool isPaused(TweenHandle handle) const;
        // Progreso normalizado sin suavizar en [0, 1] (0 si el handle es inválido)
        float progress(TweenHandle handle) const;
        std::size_t activeCount() const { return m_activeCount; }

        void clear();

    private:
        struct Slot {
            std::uint32_t generation = 0;
            bool active = false;
            bool paused = false;
            float duration = 0.0f;
            float elapsed = 0.0f;
            float delayRemaining = 0.0f;
            Ease ease = Ease::Linear;
            std::function<float(float)> customEase;
            LoopMode loopMode = LoopMode::Once;
            int loopCount = 1;
            int completedLoops = 0;
            bool reverse = false;
            std::function<void(float)> onProgress;
            std::function<void()> onComplete;
        };

        static float evaluateEase(const Slot& slot, float rawT);
        void releaseSlot(std::uint32_t index);

        std::vector<Slot> m_slots;
        std::vector<std::uint32_t> m_freeIndices;
        std::size_t m_activeCount = 0;
    };
}
