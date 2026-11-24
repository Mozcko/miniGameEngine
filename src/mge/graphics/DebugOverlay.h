#pragma once

#include <mge/graphics/Batch2D.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class Renderer2D;
    class Profiler;

    // Performance overlay (#43): FPS, frame-time graph, and named counters. Knows nothing about the game:
    // the game feeds it frame timings and any counters it wants to display. Per-frame usage:
    //   overlay.addFrame(time().unscaledDelta() * 1000, frame->acquireMs);
    //   overlay.setCounter("chunks", std::to_string(n));
    //   overlay.draw(ui);  // inside ui.begin ... ui.flush
    // SDL GPU has no timestamp queries: shows CPU time and swapchain image wait time (vsync / GPU busy),
    // not GPU execution time.
    class DebugOverlay {
    public:
        static constexpr std::size_t kHistory = 240; // Graph frames (one pixel per frame)
        static constexpr float kGraphHeight = 48.0f;
        static constexpr float kGraphMaxMs = 50.0f; // Top of the graph (taller bars are clipped)

        bool visible() const { return m_visible; }
        void setVisible(bool visible) { m_visible = visible; }
        void toggle() { m_visible = !m_visible; }

        // frameMs: total frame duration; waitMs: portion spent waiting for the swapchain image
        void addFrame(float frameMs, float waitMs = 0.0f);
        std::size_t sampleCount() const { return m_count; }
        // From history: FPS over the last ~0.5 s, average, and max
        float fps() const;
        float averageMs() const;
        float maxMs() const;
        float averageWaitMs() const;

        // "name: value" line (in insertion order; empty name = value only)
        void setCounter(std::string_view name, std::string value);
        void removeCounter(std::string_view name);
        const std::vector<std::pair<std::string, std::string>>& counters() const { return m_counters; }

        struct ProfileEntry {
            std::string name;
            float averageMs = 0.0f;
            float maxMs = 0.0f;
            int depth = 0;
        };

        // Copies entries from Profiler to draw below the counters
        void setProfile(const Profiler& profiler);
        const std::vector<ProfileEntry>& profileEntries() const { return m_profile; }

        // Builds the panel into batch using the given font (its atlas already on the GPU as atlasTexture).
        // Returns the panel size. Testable without GPU
        glm::vec2 build(Batch2D& batch, const Font& font, const Texture2D* atlasTexture,
                        const glm::vec2& pos = glm::vec2(8.0f), float scale = 1.0f) const;
        // Uses the renderer's built-in font; draws nothing if not visible
        glm::vec2 draw(Renderer2D& ui, const glm::vec2& pos = glm::vec2(8.0f), float scale = 1.0f) const;

    private:
        // i = 0 → oldest frame in history
        float sample(std::size_t i) const { return m_frameMs[(m_head + kHistory - m_count + i) % kHistory]; }

        std::array<float, kHistory> m_frameMs{};
        std::array<float, kHistory> m_waitMs{};
        std::size_t m_head = 0; // Next write position
        std::size_t m_count = 0;
        std::vector<std::pair<std::string, std::string>> m_counters;
        std::vector<ProfileEntry> m_profile;
        bool m_visible = true;
    };
}
