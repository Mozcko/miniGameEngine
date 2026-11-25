#include <mge/core/Timer.h>

#include <algorithm>
#include <utility>

namespace mge {

    // --- Timer ---

    Timer::Timer(float duration, bool repeating)
        : m_duration(duration > 0.0f ? duration : 0.0f), m_repeating(repeating) {}

    bool Timer::update(float dt) {
        return updateCount(dt) > 0;
    }

    std::uint32_t Timer::updateCount(float dt) {
        if (m_paused || m_finished || dt <= 0.0f) return 0;

        if (m_duration <= 0.0f) {
            if (!m_repeating) m_finished = true;
            return 1;
        }

        m_elapsed += dt;
        if (m_elapsed < m_duration) return 0;

        if (!m_repeating) {
            m_elapsed = m_duration;
            m_finished = true;
            return 1;
        }

        std::uint32_t fires = 0;
        while (m_elapsed >= m_duration) {
            m_elapsed -= m_duration;
            fires++;
        }
        return fires;
    }

    void Timer::reset() {
        m_elapsed = 0.0f;
        m_finished = false;
    }

    void Timer::reset(float newDuration) {
        m_duration = newDuration > 0.0f ? newDuration : 0.0f;
        reset();
    }

    float Timer::remaining() const {
        if (m_finished) return 0.0f;
        return std::max(0.0f, m_duration - m_elapsed);
    }

    float Timer::progress() const {
        if (m_duration <= 0.0f) return m_finished ? 1.0f : 0.0f;
        return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
    }

    // --- Cooldown ---

    Cooldown::Cooldown(float duration, bool startReady)
        : m_duration(duration > 0.0f ? duration : 0.0f), m_remaining(startReady ? 0.0f : m_duration) {}

    void Cooldown::update(float dt) {
        if (dt <= 0.0f || m_remaining <= 0.0f) return;
        m_remaining = std::max(0.0f, m_remaining - dt);
    }

    bool Cooldown::tryTrigger() {
        if (!isReady()) return false;
        m_remaining = m_duration;
        return true;
    }

    void Cooldown::trigger() {
        m_remaining = m_duration;
    }

    void Cooldown::setDuration(float duration) {
        m_duration = duration > 0.0f ? duration : 0.0f;
        if (m_remaining > m_duration) m_remaining = m_duration;
    }

    float Cooldown::progress() const {
        if (m_duration <= 0.0f) return 1.0f;
        return std::clamp(1.0f - m_remaining / m_duration, 0.0f, 1.0f);
    }

    // --- TimerManager ---

    TimerHandle TimerManager::after(float delay, std::function<void()> callback) {
        Entry entry;
        entry.id = m_nextId++;
        entry.interval = delay > 0.0f ? delay : 0.0f;
        entry.maxRepeats = 1;
        entry.repeating = false;
        entry.callback = std::move(callback);
        return enqueue(std::move(entry));
    }

    TimerHandle TimerManager::every(float interval, std::function<void()> callback,
                                    std::uint32_t maxRepeats) {
        Entry entry;
        entry.id = m_nextId++;
        entry.interval = interval > 0.0f ? interval : 0.0f;
        entry.maxRepeats = maxRepeats;
        entry.repeating = true;
        entry.callback = std::move(callback);
        return enqueue(std::move(entry));
    }

    TimerHandle TimerManager::enqueue(Entry entry) {
        const TimerHandle handle{entry.id};
        if (m_updating) {
            m_pending.push_back(std::move(entry));
        } else {
            m_entries.push_back(std::move(entry));
        }
        return handle;
    }

    void TimerManager::update(float dt) {
        if (dt <= 0.0f) return;

        m_updating = true;
        for (std::size_t i = 0; i < m_entries.size(); i++) {
            Entry& entry = m_entries[i];
            if (entry.cancelled || entry.finished || entry.paused) continue;

            if (entry.interval <= 0.0f) {
                entry.fireCount++;
                if (!entry.repeating || (entry.maxRepeats > 0 && entry.fireCount >= entry.maxRepeats)) {
                    entry.finished = true;
                }
                if (entry.callback) entry.callback();
                continue;
            }

            entry.elapsed += dt;
            while (!entry.cancelled && !entry.finished && !entry.paused && entry.elapsed >= entry.interval) {
                entry.elapsed -= entry.interval;
                entry.fireCount++;
                if (!entry.repeating || (entry.maxRepeats > 0 && entry.fireCount >= entry.maxRepeats)) {
                    entry.finished = true;
                }
                if (entry.callback) entry.callback();
            }
        }
        m_updating = false;

        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                           [](const Entry& e) { return e.cancelled || e.finished; }),
            m_entries.end());

        if (!m_pending.empty()) {
            for (auto& pending : m_pending) {
                if (!pending.cancelled) m_entries.push_back(std::move(pending));
            }
            m_pending.clear();
        }
    }

    TimerManager::Entry* TimerManager::findEntry(TimerHandle handle) {
        if (!handle.isValid()) return nullptr;
        for (auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    const TimerManager::Entry* TimerManager::findEntry(TimerHandle handle) const {
        if (!handle.isValid()) return nullptr;
        for (const auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (const auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    bool TimerManager::cancel(TimerHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->cancelled = true;
        return true;
    }

    bool TimerManager::pause(TimerHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = true;
        return true;
    }

    bool TimerManager::resume(TimerHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = false;
        return true;
    }

    bool TimerManager::isActive(TimerHandle handle) const {
        return findEntry(handle) != nullptr;
    }

    bool TimerManager::isPaused(TimerHandle handle) const {
        const Entry* entry = findEntry(handle);
        return entry != nullptr && entry->paused;
    }

    float TimerManager::remaining(TimerHandle handle) const {
        const Entry* entry = findEntry(handle);
        if (!entry) return 0.0f;
        return std::max(0.0f, entry->interval - entry->elapsed);
    }

    void TimerManager::clear() {
        if (m_updating) {
            for (auto& entry : m_entries) entry.cancelled = true;
            for (auto& entry : m_pending) entry.cancelled = true;
        } else {
            m_entries.clear();
            m_pending.clear();
        }
    }

    std::size_t TimerManager::activeCount() const {
        std::size_t count = 0;
        for (const auto& entry : m_entries) {
            if (!entry.cancelled && !entry.finished) count++;
        }
        for (const auto& entry : m_pending) {
            if (!entry.cancelled && !entry.finished) count++;
        }
        return count;
    }
}
