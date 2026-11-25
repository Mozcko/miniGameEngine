#include <mge/core/Tween.h>

#include <algorithm>
#include <utility>

namespace mge {

    // --- TweenSequence ---

    TweenSequence& TweenSequence::append(std::function<void(float)> apply, float duration,
                                         ease::EaseFunc easing) {
        Step step;
        step.kind = StepKind::Tween;
        step.duration = duration > 0.0f ? duration : 0.0f;
        step.easing = easing ? easing : ease::linear;
        step.apply = std::move(apply);
        m_steps.push_back(std::move(step));
        return *this;
    }

    TweenSequence& TweenSequence::appendInterval(float seconds) {
        Step step;
        step.kind = StepKind::Interval;
        step.duration = seconds > 0.0f ? seconds : 0.0f;
        m_steps.push_back(std::move(step));
        return *this;
    }

    TweenSequence& TweenSequence::appendCallback(std::function<void()> callback) {
        Step step;
        step.kind = StepKind::Callback;
        step.callback = std::move(callback);
        m_steps.push_back(std::move(step));
        return *this;
    }

    TweenSequence& TweenSequence::setDelay(float delay) {
        m_delay = delay > 0.0f ? delay : 0.0f;
        return *this;
    }

    TweenSequence& TweenSequence::setLoop(LoopMode mode, std::uint32_t repeatCount) {
        m_loopMode = mode;
        m_repeatCount = repeatCount;
        return *this;
    }

    TweenSequence& TweenSequence::onComplete(std::function<void()> callback) {
        m_onComplete = std::move(callback);
        return *this;
    }

    // --- TweenManager ---

    TweenHandle TweenManager::startSequence(TweenSequence sequence) {
        Entry entry;
        entry.id = m_nextId++;
        entry.steps = std::move(sequence.m_steps);
        entry.delayRemaining = sequence.m_delay;
        entry.loopMode = sequence.m_loopMode;
        entry.repeatCount = sequence.m_repeatCount;
        entry.onComplete = std::move(sequence.m_onComplete);
        for (const auto& step : entry.steps) {
            entry.totalDuration += step.duration;
        }

        const TweenHandle handle{entry.id};
        if (m_updating) {
            m_pending.push_back(std::move(entry));
        } else {
            m_entries.push_back(std::move(entry));
        }
        return handle;
    }

    void TweenManager::update(float dt) {
        if (dt <= 0.0f) return;

        m_updating = true;
        for (std::size_t i = 0; i < m_entries.size(); i++) {
            Entry& entry = m_entries[i];
            if (entry.cancelled || entry.finished || entry.paused) continue;
            stepEntry(entry, dt);
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

    void TweenManager::stepEntry(Entry& entry, float dt) {
        float remainingDt = dt;

        if (entry.delayRemaining > 0.0f) {
            if (remainingDt < entry.delayRemaining) {
                entry.delayRemaining -= remainingDt;
                return;
            }
            remainingDt -= entry.delayRemaining;
            entry.delayRemaining = 0.0f;
        }

        while (!entry.cancelled && !entry.finished && !entry.paused) {
            if (entry.stepIndex >= entry.steps.size()) {
                entry.completedCycles++;
                const bool done = entry.loopMode == LoopMode::Once ||
                                  (entry.repeatCount > 0 && entry.completedCycles >= entry.repeatCount);
                if (done) {
                    entry.finished = true;
                    auto cb = std::move(entry.onComplete);
                    if (cb) cb();
                    break;
                }

                entry.stepIndex = 0;
                entry.stepElapsed = 0.0f;
                if (entry.loopMode == LoopMode::PingPong) {
                    entry.reverse = !entry.reverse;
                }
                // Guard against infinite loops when all steps have zero duration.
                if (entry.totalDuration <= 0.0f) break;
                if (remainingDt <= 0.0f) break;
                continue;
            }

            const std::size_t actualIndex =
                entry.reverse ? (entry.steps.size() - 1 - entry.stepIndex) : entry.stepIndex;
            const TweenSequence::Step& step = entry.steps[actualIndex];

            if (step.kind == TweenSequence::StepKind::Callback) {
                auto cb = step.callback;
                entry.stepIndex++;
                entry.stepElapsed = 0.0f;
                if (cb) cb();
                continue;
            }

            if (step.duration <= 0.0f) {
                if (step.kind == TweenSequence::StepKind::Tween && step.apply) {
                    const float endT = entry.reverse ? 0.0f : 1.0f;
                    const float eased = step.easing ? step.easing(endT) : endT;
                    step.apply(eased);
                }
                entry.stepIndex++;
                entry.stepElapsed = 0.0f;
                continue;
            }

            const float needed = step.duration - entry.stepElapsed;
            if (remainingDt < needed) {
                entry.stepElapsed += remainingDt;
                if (step.kind == TweenSequence::StepKind::Tween && step.apply) {
                    const float rawT = std::clamp(entry.stepElapsed / step.duration, 0.0f, 1.0f);
                    const float t = entry.reverse ? (1.0f - rawT) : rawT;
                    const float eased = step.easing ? step.easing(t) : t;
                    step.apply(eased);
                }
                break;
            }

            remainingDt -= needed;
            entry.stepElapsed = 0.0f;
            entry.stepIndex++;
            if (step.kind == TweenSequence::StepKind::Tween && step.apply) {
                const float endT = entry.reverse ? 0.0f : 1.0f;
                const float eased = step.easing ? step.easing(endT) : endT;
                step.apply(eased);
            }
        }
    }

    TweenManager::Entry* TweenManager::findEntry(TweenHandle handle) {
        if (!handle.isValid()) return nullptr;
        for (auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    const TweenManager::Entry* TweenManager::findEntry(TweenHandle handle) const {
        if (!handle.isValid()) return nullptr;
        for (const auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (const auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    bool TweenManager::cancel(TweenHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->cancelled = true;
        return true;
    }

    bool TweenManager::pause(TweenHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = true;
        return true;
    }

    bool TweenManager::resume(TweenHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = false;
        return true;
    }

    bool TweenManager::isActive(TweenHandle handle) const {
        return findEntry(handle) != nullptr;
    }

    bool TweenManager::isPaused(TweenHandle handle) const {
        const Entry* entry = findEntry(handle);
        return entry != nullptr && entry->paused;
    }

    void TweenManager::clear() {
        if (m_updating) {
            for (auto& entry : m_entries) entry.cancelled = true;
            for (auto& entry : m_pending) entry.cancelled = true;
        } else {
            m_entries.clear();
            m_pending.clear();
        }
    }

    std::size_t TweenManager::activeCount() const {
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
