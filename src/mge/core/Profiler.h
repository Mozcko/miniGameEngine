#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef MGE_PROFILING
#define MGE_PROFILING 1
#endif

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#endif

namespace mge {

    // Tiempos de CPU por nombre, agregados por frame. Un Profiler por Application (sin singletons).
    // Solo hilo principal (MGE_ASSERT si se usa desde otro hilo).
    class Profiler {
    public:
        struct Entry {
            std::string name;
            float lastMs = 0.0f;     // Total del último frame terminado
            float averageMs = 0.0f;  // Promedio móvil exponencial (alpha 0.1)
            float maxMs = 0.0f;      // Máximo de las últimas ~2 s (o de kWindowFrames frames)
            std::uint32_t calls = 0; // Llamadas en el último frame
            int depth = 0;           // Anidamiento (para indentar en el overlay)
        };

        Profiler();
        ~Profiler() = default;

        Profiler(const Profiler&) = delete;
        Profiler& operator=(const Profiler&) = delete;
        Profiler(Profiler&&) = delete;
        Profiler& operator=(Profiler&&) = delete;

        void beginFrame();
        void endFrame();                   // Cierra el frame: pasa los acumulados a lastMs/averageMs
        void begin(std::string_view name); // Prefer ScopedTimer
        void end();
        const std::vector<Entry>& entries() const; // En orden de primera aparición (estable entre frames)
        void setEnabled(bool enabled);
        bool enabled() const; // Desactivado = begin/end no hacen nada (coste ~0)

    private:
        static constexpr std::size_t kWindowFrames = 120; // ~2 s a 60 FPS

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
        std::unordered_map<std::string, std::size_t> m_nameToIndex;
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
// Con Tracy activado, `name` debe ser un string literal (ej: "update") para ZoneScopedN
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
