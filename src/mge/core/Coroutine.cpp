#include <mge/core/Coroutine.h>

#include <algorithm>
#include <utility>

namespace mge {

    // --- Coroutine ---

    Coroutine& Coroutine::then(std::function<void()> action) {
        Step step;
        step.kind = StepKind::Action;
        step.action = std::move(action);
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::waitSeconds(float duration) {
        Step step;
        step.kind = StepKind::WaitSeconds;
        step.duration = duration > 0.0f ? duration : 0.0f;
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::waitFrames(std::uint32_t count) {
        Step step;
        step.kind = StepKind::WaitFrames;
        step.frameCount = count;
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::waitUntil(std::function<bool()> predicate) {
        Step step;
        step.kind = StepKind::WaitUntil;
        step.predicate = std::move(predicate);
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::waitWhile(std::function<bool()> predicate) {
        Step step;
        step.kind = StepKind::WaitWhile;
        step.predicate = std::move(predicate);
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::yieldStep(std::function<Wait(float dt)> stepFn) {
        Step step;
        step.kind = StepKind::YieldStep;
        step.yieldFn = std::move(stepFn);
        m_steps.push_back(std::move(step));
        return *this;
    }

    Coroutine& Coroutine::repeat(std::uint32_t count, const std::function<void(Coroutine&)>& builder) {
        if (!builder) return *this;
        for (std::uint32_t i = 0; i < count; i++) {
            builder(*this);
        }
        return *this;
    }

    Coroutine& Coroutine::onComplete(std::function<void()> callback) {
        m_onComplete = std::move(callback);
        return *this;
    }

    Coroutine& Coroutine::onCancel(std::function<void()> callback) {
        m_onCancel = std::move(callback);
        return *this;
    }

    // --- CoroutineRunner ---

    CoroutineHandle CoroutineRunner::start(Coroutine coroutine) {
        Entry entry;
        entry.id = m_nextId++;
        entry.steps = std::move(coroutine.m_steps);
        entry.onComplete = std::move(coroutine.m_onComplete);
        entry.onCancel = std::move(coroutine.m_onCancel);

        const CoroutineHandle handle{entry.id};
        if (m_updating) {
            m_pending.push_back(std::move(entry));
        } else {
            m_entries.push_back(std::move(entry));
        }
        return handle;
    }

    CoroutineHandle CoroutineRunner::start(std::function<Wait(float dt)> stepFn) {
        Coroutine coroutine;
        coroutine.yieldStep(std::move(stepFn));
        return start(std::move(coroutine));
    }

    void CoroutineRunner::update(float dt) {
        if (dt < 0.0f) dt = 0.0f;

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

    void CoroutineRunner::stepEntry(Entry& entry, float dt) {
        float remainingDt = dt;

        if (entry.waiting) {
            switch (entry.activeWait.kind) {
                case Wait::Kind::None:
                case Wait::Kind::Done:
                case Wait::Kind::NextFrame:
                    entry.waiting = false;
                    break;
                case Wait::Kind::Frames:
                    entry.framesWaited++;
                    if (entry.framesWaited < entry.activeWait.frameCount) return;
                    entry.waiting = false;
                    break;
                case Wait::Kind::Seconds: {
                    const float left = entry.activeWait.duration - entry.waitElapsed;
                    if (remainingDt < left) {
                        entry.waitElapsed += remainingDt;
                        return;
                    }
                    remainingDt -= left;
                    entry.waiting = false;
                    break;
                }
                case Wait::Kind::Until:
                    if (entry.activeWait.condition && !entry.activeWait.condition()) return;
                    entry.waiting = false;
                    break;
                case Wait::Kind::WhileTrue:
                    if (entry.activeWait.condition && entry.activeWait.condition()) return;
                    entry.waiting = false;
                    break;
            }
        }

        while (!entry.cancelled && !entry.finished && !entry.paused && !entry.waiting) {
            if (entry.stepIndex >= entry.steps.size()) {
                entry.finished = true;
                auto cb = std::move(entry.onComplete);
                if (cb) cb();
                break;
            }

            const Coroutine::Step& step = entry.steps[entry.stepIndex];
            switch (step.kind) {
                case Coroutine::StepKind::Action: {
                    auto action = step.action;
                    entry.stepIndex++;
                    if (action) action();
                    break;
                }
                case Coroutine::StepKind::WaitSeconds: {
                    const float waitDuration = step.duration;
                    entry.stepIndex++;
                    if (waitDuration <= 0.0f) break;
                    if (remainingDt >= waitDuration) {
                        remainingDt -= waitDuration;
                        break;
                    }
                    entry.waiting = true;
                    entry.activeWait = Wait::seconds(waitDuration);
                    entry.waitElapsed = remainingDt;
                    remainingDt = 0.0f;
                    break;
                }
                case Coroutine::StepKind::WaitFrames: {
                    const std::uint32_t count = step.frameCount;
                    entry.stepIndex++;
                    if (count == 0) break;
                    entry.waiting = true;
                    entry.activeWait = Wait::frames(count);
                    entry.framesWaited = 0;
                    break;
                }
                case Coroutine::StepKind::WaitUntil: {
                    auto pred = step.predicate;
                    entry.stepIndex++;
                    if (!pred || pred()) break;
                    entry.waiting = true;
                    entry.activeWait = Wait::until(std::move(pred));
                    break;
                }
                case Coroutine::StepKind::WaitWhile: {
                    auto pred = step.predicate;
                    entry.stepIndex++;
                    if (!pred || !pred()) break;
                    entry.waiting = true;
                    entry.activeWait = Wait::whileTrue(std::move(pred));
                    break;
                }
                case Coroutine::StepKind::YieldStep: {
                    auto fn = step.yieldFn;
                    if (!fn) {
                        entry.stepIndex++;
                        break;
                    }
                    Wait result = fn(remainingDt);
                    remainingDt = 0.0f;
                    if (entry.cancelled || entry.paused) break;
                    if (result.kind == Wait::Kind::Done) {
                        entry.stepIndex++;
                    } else if (result.kind == Wait::Kind::None) {
                        entry.waiting = true;
                        entry.activeWait = Wait::nextFrame();
                    } else if (result.kind == Wait::Kind::Frames && result.frameCount == 0) {
                        entry.waiting = true;
                        entry.activeWait = Wait::nextFrame();
                    } else if (result.kind == Wait::Kind::Seconds && result.duration <= 0.0f) {
                        entry.waiting = true;
                        entry.activeWait = Wait::nextFrame();
                    } else {
                        entry.waiting = true;
                        entry.activeWait = std::move(result);
                        entry.waitElapsed = 0.0f;
                        entry.framesWaited = 0;
                    }
                    break;
                }
            }
        }
    }

    CoroutineRunner::Entry* CoroutineRunner::findEntry(CoroutineHandle handle) {
        if (!handle.isValid()) return nullptr;
        for (auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    const CoroutineRunner::Entry* CoroutineRunner::findEntry(CoroutineHandle handle) const {
        if (!handle.isValid()) return nullptr;
        for (const auto& entry : m_entries) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        for (const auto& entry : m_pending) {
            if (entry.id == handle.id && !entry.cancelled && !entry.finished) return &entry;
        }
        return nullptr;
    }

    bool CoroutineRunner::cancel(CoroutineHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->cancelled = true;
        auto cb = std::move(entry->onCancel);
        if (cb) cb();
        return true;
    }

    bool CoroutineRunner::pause(CoroutineHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = true;
        return true;
    }

    bool CoroutineRunner::resume(CoroutineHandle handle) {
        Entry* entry = findEntry(handle);
        if (!entry) return false;
        entry->paused = false;
        return true;
    }

    bool CoroutineRunner::isRunning(CoroutineHandle handle) const {
        return findEntry(handle) != nullptr;
    }

    bool CoroutineRunner::isPaused(CoroutineHandle handle) const {
        const Entry* entry = findEntry(handle);
        return entry != nullptr && entry->paused;
    }

    void CoroutineRunner::cancelAll() {
        std::vector<std::function<void()>> callbacks;
        for (auto& entry : m_entries) {
            if (!entry.cancelled && !entry.finished) {
                entry.cancelled = true;
                if (entry.onCancel) callbacks.push_back(std::move(entry.onCancel));
            }
        }
        for (auto& entry : m_pending) {
            if (!entry.cancelled && !entry.finished) {
                entry.cancelled = true;
                if (entry.onCancel) callbacks.push_back(std::move(entry.onCancel));
            }
        }
        if (!m_updating) {
            m_entries.clear();
            m_pending.clear();
        }
        for (const auto& cb : callbacks) {
            cb();
        }
    }

    std::size_t CoroutineRunner::runningCount() const {
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
