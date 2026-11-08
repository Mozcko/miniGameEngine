#include <mge/core/Profiler.h>

#include <mge/core/Log.h>

#include <algorithm>

namespace mge {

    Profiler::Profiler() : m_mainThreadId(std::this_thread::get_id()) {}

    void Profiler::setEnabled(bool enabled) {
        m_enabled = enabled;
    }

    bool Profiler::enabled() const {
        return m_enabled;
    }

    const std::vector<Profiler::Entry>& Profiler::entries() const {
        return m_entries;
    }

    void Profiler::beginFrame() {
        if (!m_enabled) {
            return;
        }
        MGE_ASSERT(std::this_thread::get_id() == m_mainThreadId,
                   "Profiler solo debe usarse desde el hilo principal");
        m_openStack.clear();
        for (auto& acc : m_accumulators) {
            acc.totalDuration = std::chrono::steady_clock::duration{0};
            acc.calls = 0;
        }
    }

    void Profiler::endFrame() {
        if (!m_enabled) {
            return;
        }
        MGE_ASSERT(std::this_thread::get_id() == m_mainThreadId,
                   "Profiler solo debe usarse desde el hilo principal");
        MGE_ASSERT(m_openStack.empty(), "Profiler: hay scopes sin cerrar al terminar el frame");

        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            auto& entry = m_entries[i];
            auto& acc = m_accumulators[i];
            auto& hist = m_history[i];

            const float lastMs = std::chrono::duration<float, std::milli>(acc.totalDuration).count();
            entry.lastMs = lastMs;
            entry.calls = acc.calls;

            if (hist.count == 0) {
                entry.averageMs = lastMs;
            } else {
                constexpr float kAlpha = 0.1f;
                entry.averageMs = entry.averageMs + kAlpha * (lastMs - entry.averageMs);
            }

            hist.samples[hist.head] = lastMs;
            hist.head = (hist.head + 1) % kWindowFrames;
            if (hist.count < kWindowFrames) {
                hist.count++;
            }

            float maxVal = 0.0f;
            for (std::size_t s = 0; s < hist.count; ++s) {
                maxVal = std::max(maxVal, hist.samples[s]);
            }
            entry.maxMs = maxVal;

            acc.totalDuration = std::chrono::steady_clock::duration{0};
            acc.calls = 0;
        }

#if defined(TRACY_ENABLE)
        FrameMark;
#endif
    }

    void Profiler::begin(std::string_view name) {
        if (!m_enabled) {
            return;
        }
        MGE_ASSERT(std::this_thread::get_id() == m_mainThreadId,
                   "Profiler solo debe usarse desde el hilo principal");

        std::size_t index = m_entries.size();
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].name == name) {
                index = i;
                break;
            }
        }

        if (index == m_entries.size()) {
            Entry entry;
            entry.name = std::string(name);
            entry.depth = static_cast<int>(m_openStack.size());
            m_entries.push_back(std::move(entry));
            m_accumulators.push_back(Accumulator{});
            EntryHistory hist;
            hist.samples.resize(kWindowFrames, 0.0f);
            m_history.push_back(std::move(hist));
        }

        m_accumulators[index].calls++;
        m_openStack.push_back({index, std::chrono::steady_clock::now()});
    }

    void Profiler::end() {
        if (!m_enabled) {
            return;
        }
        MGE_ASSERT(std::this_thread::get_id() == m_mainThreadId,
                   "Profiler solo debe usarse desde el hilo principal");
        if (m_openStack.empty()) {
            MGE_ASSERT(false, "Profiler::end() llamado sin begin()");
            return;
        }
        const OpenScope open = m_openStack.back();
        m_openStack.pop_back();
        const auto elapsed = std::chrono::steady_clock::now() - open.startTime;
        m_accumulators[open.entryIndex].totalDuration += elapsed;
    }

} // namespace mge
