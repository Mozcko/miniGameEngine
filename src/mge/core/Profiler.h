#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef MGE_PROFILING
#define MGE_PROFILING 1
#endif

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#endif

namespace mge {

    // CPU timings by name, aggregated per frame. One Profiler per Application (no singletons).
    // Main thread only (MGE_ASSERT if used from another thread).
    class Profiler {
    public:
        struct Entry {
            std::string name;
            float lastMs = 0.0f;     // Total from the last completed frame
            float averageMs = 0.0f;  // Exponential moving average (alpha 0.1)
            float maxMs = 0.0f;      // Maximum over the last ~2 s (or kWindowFrames frames)
            std::uint32_t calls = 0; // Calls in the last frame
            int depth = 0;           // Nesting depth (for indentation in the overlay)
        };

        Profiler();
        ~Profiler() = default;

        Profiler(const Profiler&) = delete;
        Profiler& operator=(const Profiler&) = delete;
        Profiler(Profiler&&) = delete;
        Profiler& operator=(Profiler&&) = delete;

        void beginFrame();
        void endFrame();                   // Closes the frame: moves accumulators into lastMs/averageMs
        void begin(std::string_view name); // Prefer ScopedTimer
        void end();
        const std::vector<Entry>& entries() const; // In order of first appearance (stable across frames)
        void setEnabled(bool enabled);
        bool enabled() const; // Disabled = begin/end are no-ops (~0 cost)

    private:
        static constexpr std::size_t kWindowFrames = 120; // ~2 s at 60 FPS

        struct OpenScope {
            std::size_t entryIndex;
            std::chrono::steady_clock::time_point startTime;
        };

        struct Accumulator {
            std::chrono::steady_clock::duration totalDuration{0};
            std::uint32_t calls = 0;
        };

        struct EntryHistory {
            std::vector<float> samples;
            std::size_t head = 0;
            std::size_t count = 0;
        };

        std::thread::id m_mainThreadId;
        bool m_enabled = true;

        std::vector<Entry> m_entries;
        std::vector<Accumulator> m_accumulators;
        std::vector<EntryHistory> m_history;
        std::vector<OpenScope> m_openStack;
    };

    class ScopedTimer {
    public:
        ScopedTimer(Profiler& profiler, std::string_view name) : m_profiler(&profiler) {
            m_profiler->begin(name);
        }

        ScopedTimer(Profiler* profiler, std::string_view name) : m_profiler(profiler) {
            if (m_profiler) {
                m_profiler->begin(name);
            }
        }

        ~ScopedTimer() {
            if (m_profiler) {
                m_profiler->end();
            }
        }

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;

    private:
        Profiler* m_profiler = nullptr;
    };

} // namespace mge

#define MGE_CONCAT_INNER(a, b) a##b
#define MGE_CONCAT(a, b) MGE_CONCAT_INNER(a, b)

#if defined(MGE_PROFILING) && MGE_PROFILING
#if defined(TRACY_ENABLE)
// With Tracy enabled, `name` must be a string literal (e.g., "update") for ZoneScopedN
#define MGE_PROFILE_SCOPE(profiler, name)                                                                    \
    ZoneScopedN(name);                                                                                       \
    ::mge::ScopedTimer MGE_CONCAT(mgeProfileScope, __LINE__)((profiler), (name))
#else
#define MGE_PROFILE_SCOPE(profiler, name)                                                                    \
    ::mge::ScopedTimer MGE_CONCAT(mgeProfileScope, __LINE__)((profiler), (name))
#endif
#else
#define MGE_PROFILE_SCOPE(profiler, name) ((void)0)
#endif
