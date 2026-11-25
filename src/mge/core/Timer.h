#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mge {

    // Lightweight value-type countdown/repeating timer with zero heap allocations.
    // Suitable for embedding directly in ECS components or gameplay structs.
    class Timer {
    public:
        explicit Timer(float duration = 0.0f, bool repeating = false);

        // Advances the timer by dt seconds (dt <= 0 or paused is a no-op).
        // Returns true if the timer completed (fired) at least once during this update.
        bool update(float dt);

        // Advances the timer by dt seconds and returns the number of times it fired.
        // One-shot timers return at most 1; repeating timers may return > 1 if dt > duration.
        std::uint32_t updateCount(float dt);

        void reset();
        void reset(float newDuration);

        void pause() { m_paused = true; }
        void resume() { m_paused = false; }
        bool isPaused() const { return m_paused; }

        bool isFinished() const { return m_finished; }
        bool isRepeating() const { return m_repeating; }
        void setRepeating(bool repeating) { m_repeating = repeating; }

        float duration() const { return m_duration; }
        float elapsed() const { return m_elapsed; }
        float remaining() const;
        // Normalized progress in [0, 1]
        float progress() const;

    private:
        float m_duration = 0.0f;
        float m_elapsed = 0.0f;
        bool m_repeating = false;
        bool m_paused = false;
        bool m_finished = false;
    };

    // Lightweight cooldown helper (e.g., attack/dash/ability cooldowns).
    // Starts ready by default unless startReady is false.
    class Cooldown {
    public:
        explicit Cooldown(float duration = 0.0f, bool startReady = true);

        void update(float dt);

        bool isReady() const { return m_remaining <= 0.0f; }

        // If ready, starts the cooldown and returns true; otherwise returns false.
        bool tryTrigger();

        // Unconditionally resets the cooldown to its full duration.
        void trigger();

        // Immediately makes the cooldown ready.
        void reset() { m_remaining = 0.0f; }

        void setDuration(float duration);
        float duration() const { return m_duration; }
        float remaining() const { return m_remaining; }
        // Normalized readiness in [0, 1] (0 = just triggered, 1 = ready)
        float progress() const;

    private:
        float m_duration = 0.0f;
        float m_remaining = 0.0f;
    };

    // Opaque handle referencing a scheduled timer inside a TimerManager.
    struct TimerHandle {
        std::uint64_t id = 0;

        bool isValid() const { return id != 0; }
        friend bool operator==(TimerHandle a, TimerHandle b) { return a.id == b.id; }
        friend bool operator!=(TimerHandle a, TimerHandle b) { return a.id != b.id; }
    };

    inline constexpr TimerHandle kInvalidTimer{};

    // Callback-based timer scheduler. Re-entrancy safe: callbacks may schedule, pause,
    // cancel, or clear timers without invalidating iteration.
    class TimerManager {
    public:
        // Schedules a one-shot callback to run after delay seconds (<= 0 fires on next update).
        TimerHandle after(float delay, std::function<void()> callback);

        // Schedules a repeating callback every interval seconds.
        // maxRepeats == 0 repeats indefinitely until cancelled.
        TimerHandle every(float interval, std::function<void()> callback, std::uint32_t maxRepeats = 0);

        void update(float dt);

        bool cancel(TimerHandle handle);
        bool pause(TimerHandle handle);
        bool resume(TimerHandle handle);
        bool isActive(TimerHandle handle) const;
        bool isPaused(TimerHandle handle) const;
        float remaining(TimerHandle handle) const;

        void clear();
        std::size_t activeCount() const;

    private:
        struct Entry {
            std::uint64_t id = 0;
            float interval = 0.0f;
            float elapsed = 0.0f;
            std::uint32_t maxRepeats = 1; // 0 = infinite
            std::uint32_t fireCount = 0;
            bool repeating = false;
            bool paused = false;
            bool cancelled = false;
            bool finished = false;
            std::function<void()> callback;
        };

        Entry* findEntry(TimerHandle handle);
        const Entry* findEntry(TimerHandle handle) const;
        TimerHandle enqueue(Entry entry);

        std::vector<Entry> m_entries;
        std::vector<Entry> m_pending;
        std::uint64_t m_nextId = 1;
        bool m_updating = false;
    };
}
