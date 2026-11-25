#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace mge {

    // Yield instruction returned by stateful coroutine step functions.
    struct Wait {
        enum class Kind {
            None,
            NextFrame,
            Seconds,
            Frames,
            Until,
            WhileTrue,
            Done
        };

        Kind kind = Kind::None;
        float duration = 0.0f;
        std::uint32_t frameCount = 0;
        std::function<bool()> condition;

        static Wait nextFrame() { return Wait{Kind::NextFrame, 0.0f, 1, {}}; }
        static Wait seconds(float secs) { return Wait{Kind::Seconds, secs > 0.0f ? secs : 0.0f, 0, {}}; }
        static Wait frames(std::uint32_t count) { return Wait{Kind::Frames, 0.0f, count, {}}; }
        static Wait until(std::function<bool()> pred) { return Wait{Kind::Until, 0.0f, 0, std::move(pred)}; }
        static Wait whileTrue(std::function<bool()> pred) {
            return Wait{Kind::WhileTrue, 0.0f, 0, std::move(pred)};
        }
        static Wait done() { return Wait{Kind::Done, 0.0f, 0, {}}; }
    };

    class CoroutineRunner;

    // C++17 cooperative coroutine built from composable steps (actions, waits, predicates,
    // repeated blocks, and custom stateful yield steps).
    class Coroutine {
    public:
        // Runs an action synchronously and immediately advances to the next step.
        Coroutine& then(std::function<void()> action);
        // Suspends for duration seconds (<= 0 continues immediately).
        Coroutine& waitSeconds(float duration);
        // Suspends for count update ticks (0 continues immediately).
        Coroutine& waitFrames(std::uint32_t count);
        // Suspends until predicate() returns true.
        Coroutine& waitUntil(std::function<bool()> predicate);
        // Suspends while predicate() returns true.
        Coroutine& waitWhile(std::function<bool()> predicate);
        // Runs a custom stateful step each tick until it returns Wait::done().
        Coroutine& yieldStep(std::function<Wait(float dt)> step);
        // Repeats a sub-sequence built by builder count times.
        Coroutine& repeat(std::uint32_t count, const std::function<void(Coroutine&)>& builder);

        Coroutine& onComplete(std::function<void()> callback);
        Coroutine& onCancel(std::function<void()> callback);

        bool empty() const { return m_steps.empty(); }
        std::size_t stepCount() const { return m_steps.size(); }

    private:
        friend class CoroutineRunner;

        enum class StepKind {
            Action,
            WaitSeconds,
            WaitFrames,
            WaitUntil,
            WaitWhile,
            YieldStep
        };

        struct Step {
            StepKind kind = StepKind::Action;
            float duration = 0.0f;
            std::uint32_t frameCount = 0;
            std::function<void()> action;
            std::function<bool()> predicate;
            std::function<Wait(float)> yieldFn;
        };

        std::vector<Step> m_steps;
        std::function<void()> m_onComplete;
        std::function<void()> m_onCancel;
    };

    // Opaque handle referencing a running coroutine inside a CoroutineRunner.
    struct CoroutineHandle {
        std::uint64_t id = 0;

        bool isValid() const { return id != 0; }
        friend bool operator==(CoroutineHandle a, CoroutineHandle b) { return a.id == b.id; }
        friend bool operator!=(CoroutineHandle a, CoroutineHandle b) { return a.id != b.id; }
    };

    inline constexpr CoroutineHandle kInvalidCoroutine{};

    // Executes cooperative C++17 coroutines each frame. Re-entrancy safe: coroutines may
    // start, pause, cancel, or clear other coroutines during update().
    class CoroutineRunner {
    public:
        CoroutineHandle start(Coroutine coroutine);
        CoroutineHandle start(std::function<Wait(float dt)> stepFn);

        void update(float dt);

        bool cancel(CoroutineHandle handle);
        bool pause(CoroutineHandle handle);
        bool resume(CoroutineHandle handle);
        bool isRunning(CoroutineHandle handle) const;
        bool isPaused(CoroutineHandle handle) const;

        void cancelAll();
        std::size_t runningCount() const;

    private:
        struct Entry {
            std::uint64_t id = 0;
            std::vector<Coroutine::Step> steps;
            std::size_t stepIndex = 0;
            Wait activeWait;
            bool waiting = false;
            float waitElapsed = 0.0f;
            std::uint32_t framesWaited = 0;
            bool paused = false;
            bool cancelled = false;
            bool finished = false;
            std::function<void()> onComplete;
            std::function<void()> onCancel;
        };

        Entry* findEntry(CoroutineHandle handle);
        const Entry* findEntry(CoroutineHandle handle) const;
        void stepEntry(Entry& entry, float dt);

        std::vector<Entry> m_entries;
        std::vector<Entry> m_pending;
        std::uint64_t m_nextId = 1;
        bool m_updating = false;
    };
}
