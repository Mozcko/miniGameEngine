#include <mge/core/Tween.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        constexpr float kPi = 3.14159265358979323846f;

        float easeOutBounce(float t) {
            constexpr float kN1 = 7.5625f;
            constexpr float kD1 = 2.75f;
            if (t < 1.0f / kD1) {
                return kN1 * t * t;
            }
            if (t < 2.0f / kD1) {
                const float u = t - 1.5f / kD1;
                return kN1 * u * u + 0.75f;
            }
            if (t < 2.5f / kD1) {
                const float u = t - 2.25f / kD1;
                return kN1 * u * u + 0.9375f;
            }
            const float u = t - 2.625f / kD1;
            return kN1 * u * u + 0.984375f;
        }
    }

    float applyEase(Ease ease, float t) {
        const float x = std::clamp(t, 0.0f, 1.0f);
        constexpr float kC1 = 1.70158f;
        constexpr float kC2 = kC1 * 1.525f;
        constexpr float kC3 = kC1 + 1.0f;
        constexpr float kC4 = (2.0f * kPi) / 3.0f;
        constexpr float kC5 = (2.0f * kPi) / 4.5f;

        switch (ease) {
        case Ease::Linear: return x;
        case Ease::InQuad: return x * x;
        case Ease::OutQuad: return 1.0f - (1.0f - x) * (1.0f - x);
        case Ease::InOutQuad:
            return x < 0.5f ? 2.0f * x * x : 1.0f - (-2.0f * x + 2.0f) * (-2.0f * x + 2.0f) * 0.5f;
        case Ease::InCubic: return x * x * x;
        case Ease::OutCubic: {
            const float u = 1.0f - x;
            return 1.0f - u * u * u;
        }
        case Ease::InOutCubic: {
            if (x < 0.5f) return 4.0f * x * x * x;
            const float u = -2.0f * x + 2.0f;
            return 1.0f - (u * u * u) * 0.5f;
        }
        case Ease::InSine: return 1.0f - std::cos((x * kPi) * 0.5f);
        case Ease::OutSine: return std::sin((x * kPi) * 0.5f);
        case Ease::InOutSine: return -(std::cos(kPi * x) - 1.0f) * 0.5f;
        case Ease::InExpo: return x == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * x - 10.0f);
        case Ease::OutExpo: return x == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * x);
        case Ease::InOutExpo:
            if (x == 0.0f) return 0.0f;
            if (x == 1.0f) return 1.0f;
            return x < 0.5f ? std::pow(2.0f, 20.0f * x - 10.0f) * 0.5f
                            : (2.0f - std::pow(2.0f, -20.0f * x + 10.0f)) * 0.5f;
        case Ease::InBack: return kC3 * x * x * x - kC1 * x * x;
        case Ease::OutBack: {
            const float u = x - 1.0f;
            return 1.0f + kC3 * u * u * u + kC1 * u * u;
        }
        case Ease::InOutBack: {
            if (x < 0.5f) {
                const float u = 2.0f * x;
                return (u * u * ((kC2 + 1.0f) * 2.0f * x - kC2)) * 0.5f;
            }
            const float u = 2.0f * x - 2.0f;
            return (u * u * ((kC2 + 1.0f) * u + kC2) + 2.0f) * 0.5f;
        }
        case Ease::InBounce: return 1.0f - easeOutBounce(1.0f - x);
        case Ease::OutBounce: return easeOutBounce(x);
        case Ease::InOutBounce:
            return x < 0.5f ? (1.0f - easeOutBounce(1.0f - 2.0f * x)) * 0.5f
                            : (1.0f + easeOutBounce(2.0f * x - 1.0f)) * 0.5f;
        case Ease::InElastic:
            if (x == 0.0f) return 0.0f;
            if (x == 1.0f) return 1.0f;
            return -std::pow(2.0f, 10.0f * x - 10.0f) * std::sin((x * 10.0f - 10.75f) * kC4);
        case Ease::OutElastic:
            if (x == 0.0f) return 0.0f;
            if (x == 1.0f) return 1.0f;
            return std::pow(2.0f, -10.0f * x) * std::sin((x * 10.0f - 0.75f) * kC4) + 1.0f;
        case Ease::InOutElastic:
            if (x == 0.0f) return 0.0f;
            if (x == 1.0f) return 1.0f;
            if (x < 0.5f) {
                return -(std::pow(2.0f, 20.0f * x - 10.0f) * std::sin((20.0f * x - 11.125f) * kC5)) *
                       0.5f;
            }
            return (std::pow(2.0f, -20.0f * x + 10.0f) * std::sin((20.0f * x - 11.125f) * kC5)) * 0.5f +
                   1.0f;
        }
        return x;
    }

    TweenHandle TweenManager::add(float durationSeconds, std::function<void(float)> onProgress, Ease ease) {
        if (!onProgress) return kNullTween;

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
        slot.duration = durationSeconds > 0.0f ? durationSeconds : 0.0f;
        slot.elapsed = 0.0f;
        slot.delayRemaining = 0.0f;
        slot.ease = ease;
        slot.customEase = nullptr;
        slot.loopMode = LoopMode::Once;
        slot.loopCount = 1;
        slot.completedLoops = 0;
        slot.reverse = false;
        slot.onProgress = std::move(onProgress);
        slot.onComplete = nullptr;
        m_activeCount++;
        return TweenHandle{index, slot.generation};
    }

    void TweenManager::setDelay(TweenHandle handle, float delaySeconds) {
        if (!isActive(handle)) return;
        m_slots[handle.index].delayRemaining = delaySeconds > 0.0f ? delaySeconds : 0.0f;
    }

    void TweenManager::setLoop(TweenHandle handle, LoopMode mode, int loopCount) {
        if (!isActive(handle)) return;
        Slot& slot = m_slots[handle.index];
        slot.loopMode = mode;
        if (mode == LoopMode::Once) {
            slot.loopCount = 1;
        } else {
            slot.loopCount = loopCount == 0 ? 1 : loopCount;
        }
    }

    void TweenManager::setEase(TweenHandle handle, Ease ease) {
        if (!isActive(handle)) return;
        m_slots[handle.index].ease = ease;
        m_slots[handle.index].customEase = nullptr;
    }

    void TweenManager::setEasingFunction(TweenHandle handle, std::function<float(float)> customEase) {
        if (!isActive(handle)) return;
        m_slots[handle.index].customEase = std::move(customEase);
    }

    void TweenManager::setOnComplete(TweenHandle handle, std::function<void()> callback) {
        if (!isActive(handle)) return;
        m_slots[handle.index].onComplete = std::move(callback);
    }

    void TweenManager::update(float dt) {
        if (dt < 0.0f || m_activeCount == 0) return;

        std::vector<TweenHandle> activeHandles;
        activeHandles.reserve(m_activeCount);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            if (m_slots[i].active && !m_slots[i].paused) {
                activeHandles.push_back(TweenHandle{i, m_slots[i].generation});
            }
        }

        for (TweenHandle handle : activeHandles) {
            if (!isActive(handle) || m_slots[handle.index].paused) continue;

            Slot& slot = m_slots[handle.index];
            float stepDt = dt;

            if (slot.delayRemaining > 0.0f) {
                if (stepDt < slot.delayRemaining) {
                    slot.delayRemaining -= stepDt;
                    continue;
                }
                stepDt -= slot.delayRemaining;
                slot.delayRemaining = 0.0f;
            }

            if (slot.duration <= 0.0f) {
                const float finalT = evaluateEase(slot, 1.0f);
                auto progressCb = slot.onProgress;
                auto completeCb = slot.onComplete;
                releaseSlot(handle.index);
                if (progressCb) progressCb(finalT);
                if (completeCb) completeCb();
                continue;
            }

            slot.elapsed += stepDt;
            bool finished = false;
            float rawT = 0.0f;

            if (slot.elapsed >= slot.duration) {
                if (slot.loopMode == LoopMode::Once) {
                    finished = true;
                    rawT = 1.0f;
                } else {
                    const int passes = static_cast<int>(slot.elapsed / slot.duration);
                    if (slot.loopCount > 0 && slot.completedLoops + passes >= slot.loopCount) {
                        finished = true;
                        if (slot.loopMode == LoopMode::PingPong) {
                            rawT = ((slot.loopCount - 1) % 2 == 0) ? 1.0f : 0.0f;
                        } else {
                            rawT = 1.0f;
                        }
                    } else {
                        slot.completedLoops += passes;
                        if (slot.loopMode == LoopMode::PingPong && (passes % 2 != 0)) {
                            slot.reverse = !slot.reverse;
                        }
                        slot.elapsed = std::fmod(slot.elapsed, slot.duration);
                        const float norm = slot.elapsed / slot.duration;
                        rawT = slot.reverse ? (1.0f - norm) : norm;
                    }
                }
            } else {
                const float norm = slot.elapsed / slot.duration;
                rawT = slot.reverse ? (1.0f - norm) : norm;
            }

            const float easedT = evaluateEase(slot, rawT);
            auto progressCb = slot.onProgress;
            auto completeCb = finished ? slot.onComplete : std::function<void()>{};

            if (finished) releaseSlot(handle.index);

            if (progressCb) progressCb(easedT);
            if (completeCb) completeCb();
        }
    }

    void TweenManager::cancel(TweenHandle handle) {
        if (!isActive(handle)) return;
        releaseSlot(handle.index);
    }

    void TweenManager::pause(TweenHandle handle) {
        if (!isActive(handle)) return;
        m_slots[handle.index].paused = true;
    }

    void TweenManager::resume(TweenHandle handle) {
        if (!isActive(handle)) return;
        m_slots[handle.index].paused = false;
    }

    bool TweenManager::isActive(TweenHandle handle) const {
        if (handle.isNull() || handle.index >= m_slots.size()) return false;
        const Slot& slot = m_slots[handle.index];
        return slot.active && slot.generation == handle.generation;
    }

    bool TweenManager::isPaused(TweenHandle handle) const {
        return isActive(handle) && m_slots[handle.index].paused;
    }

    float TweenManager::progress(TweenHandle handle) const {
        if (!isActive(handle)) return 0.0f;
        const Slot& slot = m_slots[handle.index];
        if (slot.duration <= 0.0f) return 1.0f;
        const float norm = std::clamp(slot.elapsed / slot.duration, 0.0f, 1.0f);
        return slot.reverse ? (1.0f - norm) : norm;
    }

    void TweenManager::clear() {
        m_freeIndices.clear();
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            Slot& slot = m_slots[i];
            if (slot.active) slot.generation++;
            slot.active = false;
            slot.paused = false;
            slot.customEase = nullptr;
            slot.onProgress = nullptr;
            slot.onComplete = nullptr;
            m_freeIndices.push_back(i);
        }
        m_activeCount = 0;
    }

    float TweenManager::evaluateEase(const Slot& slot, float rawT) {
        const float clamped = std::clamp(rawT, 0.0f, 1.0f);
        if (slot.customEase) return slot.customEase(clamped);
        return applyEase(slot.ease, clamped);
    }

    void TweenManager::releaseSlot(std::uint32_t index) {
        Slot& slot = m_slots[index];
        if (!slot.active) return;
        slot.active = false;
        slot.paused = false;
        slot.generation++;
        slot.customEase = nullptr;
        slot.onProgress = nullptr;
        slot.onComplete = nullptr;
        m_freeIndices.push_back(index);
        m_activeCount--;
    }
}
