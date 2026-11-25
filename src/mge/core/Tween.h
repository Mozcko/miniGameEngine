#pragma once

#include <mge/core/Easing.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace mge {

    // Generic interpolation for tweenable types (float, double, glm::vec2/3/4, etc.).
    // Does not clamp t so overshoot easings (Back, Elastic) work naturally.
    template <typename T> T tweenLerp(const T& a, const T& b, float t) {
        return a + (b - a) * t;
    }

    // Quaternion interpolation using spherical linear interpolation (slerp).
    inline glm::quat tweenLerp(const glm::quat& a, const glm::quat& b, float t) {
        return glm::slerp(a, b, t);
    }

    enum class LoopMode {
        Once,
        Loop,
        PingPong
    };

    // Opaque handle referencing an active tween or sequence in a TweenManager.
    struct TweenHandle {
        std::uint64_t id = 0;

        bool isValid() const { return id != 0; }
        friend bool operator==(TweenHandle a, TweenHandle b) { return a.id == b.id; }
        friend bool operator!=(TweenHandle a, TweenHandle b) { return a.id != b.id; }
    };

    inline constexpr TweenHandle kInvalidTween{};

    // Configuration for a single tween.
    struct TweenOptions {
        ease::EaseFunc easing = ease::linear;
        float delay = 0.0f;
        LoopMode loopMode = LoopMode::Once;
        // Total cycles for Loop / PingPong (0 = infinite). Ignored for LoopMode::Once.
        std::uint32_t repeatCount = 0;
        std::function<void()> onComplete;
    };

    class TweenManager;

    // Sequential chain of tweens, wait intervals, and callbacks played by a TweenManager.
    class TweenSequence {
    public:
        TweenSequence& append(std::function<void(float)> apply, float duration,
                              ease::EaseFunc easing = ease::linear);

        template <typename T>
        TweenSequence& append(T from, T to, float duration, std::function<void(const T&)> setter,
                              ease::EaseFunc easing = ease::linear) {
            return append(
                [a = std::move(from), b = std::move(to), fn = std::move(setter)](float t) {
                    if (fn) fn(tweenLerp(a, b, t));
                },
                duration, easing);
        }

        TweenSequence& appendInterval(float seconds);
        TweenSequence& appendCallback(std::function<void()> callback);
        TweenSequence& setDelay(float delay);
        TweenSequence& setLoop(LoopMode mode, std::uint32_t repeatCount = 0);
        TweenSequence& onComplete(std::function<void()> callback);

        bool empty() const { return m_steps.empty(); }
        std::size_t stepCount() const { return m_steps.size(); }

    private:
        friend class TweenManager;

        enum class StepKind {
            Tween,
            Interval,
            Callback
        };

        struct Step {
            StepKind kind = StepKind::Tween;
            float duration = 0.0f;
            ease::EaseFunc easing = ease::linear;
            std::function<void(float)> apply;
            std::function<void()> callback;
        };

        std::vector<Step> m_steps;
        float m_delay = 0.0f;
        LoopMode m_loopMode = LoopMode::Once;
        std::uint32_t m_repeatCount = 0;
        std::function<void()> m_onComplete;
    };

    // Manages active tweens and sequences. Re-entrancy safe: callbacks may start, pause,
    // cancel, or clear tweens without invalidating iteration.
    class TweenManager {
    public:
        // Callback-driven tween interpolating from -> to over duration seconds.
        template <typename T>
        TweenHandle tween(T from, T to, float duration, std::function<void(const T&)> setter,
                          TweenOptions options = {}) {
            TweenSequence sequence;
            sequence.append(std::move(from), std::move(to), duration, std::move(setter), options.easing);
            sequence.setDelay(options.delay);
            sequence.setLoop(options.loopMode, options.repeatCount);
            sequence.onComplete(std::move(options.onComplete));
            return startSequence(std::move(sequence));
        }

        // Convenience tween writing directly into target. Target must outlive the tween.
        template <typename T>
        TweenHandle tweenTo(T& target, T to, float duration, TweenOptions options = {}) {
            T from = target;
            return tween(
                std::move(from), std::move(to), duration,
                [&target](const T& value) { target = value; }, std::move(options));
        }

        // Starts a multi-step TweenSequence.
        TweenHandle startSequence(TweenSequence sequence);

        void update(float dt);

        bool cancel(TweenHandle handle);
        bool pause(TweenHandle handle);
        bool resume(TweenHandle handle);
        bool isActive(TweenHandle handle) const;
        bool isPaused(TweenHandle handle) const;

        void clear();
        std::size_t activeCount() const;

    private:
        struct Entry {
            std::uint64_t id = 0;
            std::vector<TweenSequence::Step> steps;
            float delayRemaining = 0.0f;
            LoopMode loopMode = LoopMode::Once;
            std::uint32_t repeatCount = 0;
            std::uint32_t completedCycles = 0;
            std::size_t stepIndex = 0;
            float stepElapsed = 0.0f;
            float totalDuration = 0.0f;
            bool reverse = false;
            bool paused = false;
            bool cancelled = false;
            bool finished = false;
            std::function<void()> onComplete;
        };

        Entry* findEntry(TweenHandle handle);
        const Entry* findEntry(TweenHandle handle) const;
        void stepEntry(Entry& entry, float dt);

        std::vector<Entry> m_entries;
        std::vector<Entry> m_pending;
        std::uint64_t m_nextId = 1;
        bool m_updating = false;
    };
}
