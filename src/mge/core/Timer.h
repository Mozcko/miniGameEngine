#pragma once

#include <algorithm>
#include <cmath>

namespace mge {

    // Countdown/elapsed timer for gameplay logic (delays, waves, buffs, spawns).
    // Advance each frame or fixed step with tick(dt) or tickCount(dt).
    class Timer {
    public:
        Timer() = default;

        explicit Timer(float duration, bool repeat = false)
            : m_duration(clampNonNegative(duration)), m_repeat(repeat), m_running(true) {}

        static Timer oneShot(float duration) { return Timer(duration, false); }
        static Timer repeating(float duration) { return Timer(duration, true); }

        // Advances the timer by dt seconds. Returns how many times it triggered on this tick
        // (0 or 1 for one-shot; 0..N for repeating, preserving overflow so it does not drift).
        // Does nothing when paused/stopped or when dt <= 0.
        int tickCount(float dt) {
            if (!m_running || !(dt > 0.0f)) return 0;
            if (m_duration <= 0.0f) {
                if (!m_repeat) {
                    m_finished = true;
                    m_running = false;
                }
                return 1;
            }
            m_elapsed += dt;
            if (m_elapsed < m_duration) return 0;
            if (!m_repeat) {
                m_elapsed = m_duration;
                m_finished = true;
                m_running = false;
                return 1;
            }
            const float periods = std::floor(m_elapsed / m_duration);
            const int count = periods >= 1.0f ? static_cast<int>(periods) : 1;
            m_elapsed -= static_cast<float>(count) * m_duration;
            if (m_elapsed < 0.0f) m_elapsed = 0.0f;
            return count;
        }

        // Advances the timer by dt seconds. Returns true if it triggered at least once.
        bool tick(float dt) { return tickCount(dt) > 0; }

        void start() {
            if (m_finished) {
                m_elapsed = 0.0f;
                m_finished = false;
            }
            m_running = true;
        }

        void start(float duration) {
            m_duration = clampNonNegative(duration);
            m_elapsed = 0.0f;
            m_finished = false;
            m_running = true;
        }

        void pause() { m_running = false; }

        void resume() {
            if (!m_finished) m_running = true;
        }

        void stop() {
            m_running = false;
            m_elapsed = 0.0f;
            m_finished = false;
        }

        void reset() {
            m_elapsed = 0.0f;
            m_finished = false;
            m_running = true;
        }

        void reset(float duration) { start(duration); }

        void finish() {
            m_elapsed = m_duration;
            m_finished = true;
            m_running = false;
        }

        void setDuration(float duration) {
            m_duration = clampNonNegative(duration);
            if (m_elapsed > m_duration) m_elapsed = m_duration;
        }

        void setRepeating(bool repeat) { m_repeat = repeat; }

        float duration() const { return m_duration; }
        float elapsed() const { return m_elapsed; }
        float remaining() const { return std::max(0.0f, m_duration - m_elapsed); }

        // Normalized progress in [0, 1].
        float progress() const {
            if (m_duration <= 0.0f) return m_finished ? 1.0f : 0.0f;
            return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
        }

        bool isRunning() const { return m_running; }
        bool isFinished() const { return m_finished; }
        bool isRepeating() const { return m_repeat; }

    private:
        static float clampNonNegative(float value) { return value > 0.0f ? value : 0.0f; }

        float m_duration = 0.0f;
        float m_elapsed = 0.0f;
        bool m_repeat = false;
        bool m_running = false;
        bool m_finished = false;
    };

    // Rate-limiter for actions and abilities (weapon fire, dash, block break).
    // Starts ready by default; call tryUse() to consume and tick(dt) each frame to recharge.
    class Cooldown {
    public:
        explicit Cooldown(float duration = 0.0f, bool startReady = true)
            : m_duration(clampNonNegative(duration)), m_remaining(startReady ? 0.0f : m_duration) {}

        // Advances the cooldown by dt seconds. Returns true on the tick it becomes ready.
        // Does nothing when already ready or when dt <= 0.
        bool tick(float dt) {
            if (m_remaining <= 0.0f || !(dt > 0.0f)) return false;
            m_remaining -= dt;
            if (m_remaining <= 0.0f) {
                m_remaining = 0.0f;
                return true;
            }
            return false;
        }

        // Consumes the cooldown if ready, restarting it for full duration.
        bool tryUse() {
            if (!isReady()) return false;
            m_remaining = m_duration;
            return true;
        }

        // Unconditionally puts the cooldown on full duration.
        void trigger() { m_remaining = m_duration; }

        // Immediately marks the cooldown as ready.
        void reset() { m_remaining = 0.0f; }

        void setDuration(float duration) {
            m_duration = clampNonNegative(duration);
            if (m_remaining > m_duration) m_remaining = m_duration;
        }

        bool isReady() const { return m_remaining <= 0.0f; }
        float duration() const { return m_duration; }
        float remaining() const { return m_remaining; }
        float elapsed() const { return std::max(0.0f, m_duration - m_remaining); }

        // Normalized readiness in [0, 1] (0 = just triggered, 1 = ready).
        float progress() const {
            if (m_duration <= 0.0f) return 1.0f;
            return std::clamp(1.0f - (m_remaining / m_duration), 0.0f, 1.0f);
        }

    private:
        static float clampNonNegative(float value) { return value > 0.0f ? value : 0.0f; }

        float m_duration = 0.0f;
        float m_remaining = 0.0f;
    };
}
