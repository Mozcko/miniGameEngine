#include <mge/core/Timer.h>

#include <utility>

namespace mge {

    TimerHandle TimerManager::after(float delaySeconds, std::function<void()> callback) {
        if (!callback) return kNullTimer;
        const float delay = delaySeconds > 0.0f ? delaySeconds : 0.0f;
        return allocateSlot(delay, 0.0f, 1, std::move(callback));
    }

    TimerHandle TimerManager::every(float intervalSeconds, std::function<void()> callback, int repeatCount) {
        if (!callback || intervalSeconds <= 0.0f || repeatCount == 0) return kNullTimer;
        return allocateSlot(intervalSeconds, intervalSeconds, repeatCount, std::move(callback));
    }

    void TimerManager::update(float dt) {
        if (dt < 0.0f || m_activeCount == 0) return;

        // Instantánea de los handles activos: crear o cancelar temporizadores en callbacks es seguro
        std::vector<TimerHandle> activeHandles;
        activeHandles.reserve(m_activeCount);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            if (m_slots[i].active && !m_slots[i].paused) {
                activeHandles.push_back(TimerHandle{i, m_slots[i].generation});
            }
        }

        for (TimerHandle handle : activeHandles) {
            if (!isActive(handle) || m_slots[handle.index].paused) continue;

            m_slots[handle.index].remaining -= dt;
            while (isActive(handle) && !m_slots[handle.index].paused &&
                   m_slots[handle.index].remaining <= 0.0f) {
                Slot& slot = m_slots[handle.index];
                auto callback = slot.callback;

                if (slot.interval <= 0.0f) {
                    releaseSlot(handle.index);
                    callback();
                    break;
                }

                if (slot.remainingRepeats > 0) {
                    slot.remainingRepeats--;
                    if (slot.remainingRepeats == 0) {
                        releaseSlot(handle.index);
                        callback();
                        break;
                    }
                }

                slot.remaining += slot.interval;
                callback();
            }
        }
    }

    void TimerManager::cancel(TimerHandle handle) {
        if (!isActive(handle)) return;
        releaseSlot(handle.index);
    }

    void TimerManager::pause(TimerHandle handle) {
        if (!isActive(handle)) return;
        m_slots[handle.index].paused = true;
    }

    void TimerManager::resume(TimerHandle handle) {
        if (!isActive(handle)) return;
        m_slots[handle.index].paused = false;
    }

    bool TimerManager::isActive(TimerHandle handle) const {
        if (handle.isNull() || handle.index >= m_slots.size()) return false;
        const Slot& slot = m_slots[handle.index];
        return slot.active && slot.generation == handle.generation;
    }

    bool TimerManager::isPaused(TimerHandle handle) const {
        return isActive(handle) && m_slots[handle.index].paused;
    }

    float TimerManager::remaining(TimerHandle handle) const {
        if (!isActive(handle)) return 0.0f;
        const float value = m_slots[handle.index].remaining;
        return value > 0.0f ? value : 0.0f;
    }

    void TimerManager::clear() {
        m_freeIndices.clear();
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            Slot& slot = m_slots[i];
            if (slot.active) slot.generation++;
            slot.active = false;
            slot.paused = false;
            slot.callback = nullptr;
            m_freeIndices.push_back(i);
        }
        m_activeCount = 0;
    }

    TimerHandle TimerManager::allocateSlot(float delaySeconds, float intervalSeconds, int repeatCount,
                                           std::function<void()> callback) {
        std::uint32_t index = 0;
        if (!m_freeIndices.empty()) {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
        } else {
            index = static_cast<std::uint32_t>(m_slots.size());
            m_slots.emplace_back();
        }

        Slot& slot = m_slots[index];
        slot.active = true;
        slot.paused = false;
        slot.remaining = delaySeconds;
        slot.interval = intervalSeconds;
        slot.remainingRepeats = repeatCount;
        slot.callback = std::move(callback);
        m_activeCount++;
        return TimerHandle{index, slot.generation};
    }

    void TimerManager::releaseSlot(std::uint32_t index) {
        Slot& slot = m_slots[index];
        if (!slot.active) return;
        slot.active = false;
        slot.paused = false;
        slot.generation++;
        slot.callback = nullptr;
        m_freeIndices.push_back(index);
        m_activeCount--;
    }
}
