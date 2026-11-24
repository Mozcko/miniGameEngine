#include <mge/core/Coroutine.h>

#include <memory>
#include <utility>

namespace mge {

    CoroutineSequence& CoroutineSequence::run(std::function<void()> action) {
        m_steps.push_back([fn = std::move(action)]() {
            if (fn) fn();
            return Wait::immediate();
        });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::yieldFrame() {
        m_steps.push_back([]() { return Wait::none(); });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::waitSeconds(float duration) {
        m_steps.push_back([duration]() { return Wait::seconds(duration); });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::waitFrames(std::uint32_t count) {
        m_steps.push_back([count]() { return Wait::frames(count); });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::waitUntil(std::function<bool()> predicate) {
        m_steps.push_back([pred = std::move(predicate)]() { return Wait::until(pred); });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::waitWhile(std::function<bool()> predicate) {
        m_steps.push_back([pred = std::move(predicate)]() { return Wait::whileTrue(pred); });
        return *this;
    }

    CoroutineSequence& CoroutineSequence::step(CoroutineStep customStep) {
        if (customStep) m_steps.push_back(std::move(customStep));
        return *this;
    }

    CoroutineStep CoroutineSequence::build() && {
        struct SequenceState {
            std::vector<CoroutineStep> steps;
            std::size_t current = 0;
        };
        auto state = std::make_shared<SequenceState>();
        state->steps = std::move(m_steps);
        return [state]() -> YieldInstruction {
            if (state->current >= state->steps.size()) return Wait::done();
            return state->steps[state->current++]();
        };
    }

    CoroutineStep CoroutineSequence::build() const& {
        CoroutineSequence copy = *this;
        return std::move(copy).build();
    }

    CoroutineHandle CoroutineRunner::start(CoroutineStep step) {
        if (!step) return kNullCoroutine;

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
        slot.started = false;
        slot.wait = Wait::immediate();
        slot.step = std::move(step);
        m_runningCount++;
        return CoroutineHandle{index, slot.generation};
    }

    CoroutineHandle CoroutineRunner::start(CoroutineSequence sequence) {
        return start(std::move(sequence).build());
    }

    void CoroutineRunner::update(float dt) {
        if (dt < 0.0f || m_runningCount == 0) return;

        std::vector<CoroutineHandle> activeHandles;
        activeHandles.reserve(m_runningCount);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            if (m_slots[i].active && !m_slots[i].paused) {
                activeHandles.push_back(CoroutineHandle{i, m_slots[i].generation});
            }
        }

        for (CoroutineHandle handle : activeHandles) {
            if (!isRunning(handle) || m_slots[handle.index].paused) continue;

            Slot& slot = m_slots[handle.index];
            bool ready = false;

            if (!slot.started) {
                slot.started = true;
                ready = true;
            } else {
                switch (slot.wait.type) {
                case YieldType::Immediate:
                case YieldType::None: ready = true; break;
                case YieldType::Seconds:
                    slot.wait.seconds -= dt;
                    ready = slot.wait.seconds <= 0.0f;
                    break;
                case YieldType::Frames:
                    if (slot.wait.frames > 0) slot.wait.frames--;
                    ready = slot.wait.frames == 0;
                    break;
                case YieldType::Until: {
                    auto pred = slot.wait.predicate;
                    ready = !pred || pred();
                    break;
                }
                case YieldType::WhileTrue: {
                    auto pred = slot.wait.predicate;
                    ready = !pred || !pred();
                    break;
                }
                case YieldType::Done:
                    releaseSlot(handle.index);
                    ready = false;
                    break;
                }
            }

            if (ready && isRunning(handle) && !m_slots[handle.index].paused) {
                advanceCoroutine(handle);
            }
        }
    }

    void CoroutineRunner::advanceCoroutine(CoroutineHandle handle) {
        // Límite defensivo por tick para evitar bucles infinitos en pasos mal formados
        constexpr int kMaxStepsPerUpdate = 1024;
        for (int iteration = 0; iteration < kMaxStepsPerUpdate; ++iteration) {
            if (!isRunning(handle) || m_slots[handle.index].paused) return;

            auto stepFn = m_slots[handle.index].step;
            YieldInstruction next = stepFn();

            if (!isRunning(handle)) return;

            switch (next.type) {
            case YieldType::Done: releaseSlot(handle.index); return;
            case YieldType::Immediate: continue;
            case YieldType::None: m_slots[handle.index].wait = std::move(next); return;
            case YieldType::Seconds:
                if (next.seconds <= 0.0f) continue;
                m_slots[handle.index].wait = std::move(next);
                return;
            case YieldType::Frames:
                if (next.frames == 0) continue;
                m_slots[handle.index].wait = std::move(next);
                return;
            case YieldType::Until: {
                auto pred = next.predicate;
                if (!pred || pred()) continue;
                if (!isRunning(handle)) return;
                m_slots[handle.index].wait = std::move(next);
                return;
            }
            case YieldType::WhileTrue: {
                auto pred = next.predicate;
                if (!pred || !pred()) continue;
                if (!isRunning(handle)) return;
                m_slots[handle.index].wait = std::move(next);
                return;
            }
            }
        }
    }

    void CoroutineRunner::stop(CoroutineHandle handle) {
        if (!isRunning(handle)) return;
        releaseSlot(handle.index);
    }

    void CoroutineRunner::pause(CoroutineHandle handle) {
        if (!isRunning(handle)) return;
        m_slots[handle.index].paused = true;
    }

    void CoroutineRunner::resume(CoroutineHandle handle) {
        if (!isRunning(handle)) return;
        m_slots[handle.index].paused = false;
    }

    bool CoroutineRunner::isRunning(CoroutineHandle handle) const {
        if (handle.isNull() || handle.index >= m_slots.size()) return false;
        const Slot& slot = m_slots[handle.index];
        return slot.active && slot.generation == handle.generation;
    }

    bool CoroutineRunner::isPaused(CoroutineHandle handle) const {
        return isRunning(handle) && m_slots[handle.index].paused;
    }

    void CoroutineRunner::clear() {
        m_freeIndices.clear();
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_slots.size()); ++i) {
            Slot& slot = m_slots[i];
            if (slot.active) slot.generation++;
            slot.active = false;
            slot.paused = false;
            slot.started = false;
            slot.wait = Wait::done();
            slot.step = nullptr;
            m_freeIndices.push_back(i);
        }
        m_runningCount = 0;
    }

    void CoroutineRunner::releaseSlot(std::uint32_t index) {
        Slot& slot = m_slots[index];
        if (!slot.active) return;
        slot.active = false;
        slot.paused = false;
        slot.started = false;
        slot.generation++;
        slot.wait = Wait::done();
        slot.step = nullptr;
        m_freeIndices.push_back(index);
        m_runningCount--;
    }
}
