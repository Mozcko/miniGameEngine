#include <mge/graphics/DebugOverlay.h>

#include <mge/graphics/Renderer2D.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace mge {

    namespace {
        constexpr float kFpsWindowMs = 500.0f;
        constexpr float kTarget60Ms = 1000.0f / 60.0f;
        constexpr float kTarget30Ms = 1000.0f / 30.0f;

        std::string fixed1(float value) {
            std::ostringstream out;
            out << std::fixed << std::setprecision(1) << value;
            return out.str();
        }

        // Verde a 60 FPS o más, amarillo hasta 30, rojo por debajo
        glm::vec4 barColor(float ms) {
            if (ms <= kTarget60Ms) return {0.35f, 0.9f, 0.4f, 0.9f};
            if (ms <= kTarget30Ms) return {1.0f, 0.85f, 0.25f, 0.9f};
            return {1.0f, 0.3f, 0.25f, 0.95f};
        }
    }

    void DebugOverlay::addFrame(float frameMs, float waitMs) {
        m_frameMs[m_head] = std::max(frameMs, 0.0f);
        m_waitMs[m_head] = std::clamp(waitMs, 0.0f, m_frameMs[m_head]);
        m_head = (m_head + 1) % kHistory;
        m_count = std::min(m_count + 1, kHistory);
    }

    float DebugOverlay::fps() const {
        float total = 0.0f;
        std::size_t frames = 0;
        for (std::size_t i = m_count; i-- > 0 && total < kFpsWindowMs;) {
            total += sample(i);
            frames++;
        }
        return total > 0.0f ? 1000.0f * float(frames) / total : 0.0f;
    }

    float DebugOverlay::averageMs() const {
        if (m_count == 0) return 0.0f;
        float total = 0.0f;
        for (std::size_t i = 0; i < m_count; i++) total += sample(i);
        return total / float(m_count);
    }

    float DebugOverlay::maxMs() const {
        float worst = 0.0f;
        for (std::size_t i = 0; i < m_count; i++) worst = std::max(worst, sample(i));
        return worst;
    }

    float DebugOverlay::averageWaitMs() const {
        if (m_count == 0) return 0.0f;
        float total = 0.0f;
        for (std::size_t i = 0; i < m_count; i++)
            total += m_waitMs[(m_head + kHistory - m_count + i) % kHistory];
        return total / float(m_count);
    }

    void DebugOverlay::setCounter(std::string_view name, std::string value) {
        for (auto& [key, current] : m_counters)
            if (key == name) {
                current = std::move(value);
                return;
            }
        m_counters.emplace_back(std::string(name), std::move(value));
    }

    void DebugOverlay::removeCounter(std::string_view name) {
        m_counters.erase(std::remove_if(m_counters.begin(), m_counters.end(),
                                        [&](const auto& counter) { return counter.first == name; }),
                         m_counters.end());
    }

    glm::vec2 DebugOverlay::build(Batch2D& batch, const Font& font, const Texture2D* atlasTexture,
                                  const glm::vec2& pos, float scale) const {
        if (!font.valid()) return glm::vec2(0.0f);
        // Textos primero: el fondo se dibuja antes y tiene que medirlos
        const float average = averageMs();
        const float wait = averageWaitMs();
        std::vector<std::string> header = {"FPS " + std::to_string(int(std::lround(fps()))) + "   " +
                                               fixed1(average) + " ms (max " + fixed1(maxMs()) + ")",
                                           "CPU " + fixed1(std::max(average - wait, 0.0f)) + " ms   espera " +
                                               fixed1(wait) + " ms"};
        std::vector<std::string> lines;
        lines.reserve(m_counters.size());
        for (const auto& [name, value] : m_counters)
            lines.push_back(name.empty() ? value : name + ": " + value);

        const float pad = 4.0f * scale;
        const float lineHeight = font.lineHeight() * scale;
        const glm::vec2 graphSize(float(kHistory) * scale, kGraphHeight * scale);
        float width = graphSize.x;
        for (const auto* group : {&header, &lines})
            for (const std::string& line : *group) width = std::max(width, font.measure(line, scale).x);
        const float height = float(header.size() + lines.size()) * lineHeight + graphSize.y + pad;
        const glm::vec2 size = glm::vec2(width, height) + 2.0f * pad;

        batch.rect(pos, size, {0.0f, 0.0f, 0.0f, 0.6f});
        glm::vec2 pen = pos + pad;
        const glm::vec4 headerColor(1.0f, 0.95f, 0.5f, 1.0f);
        for (const std::string& line : header) {
            batch.text(font, atlasTexture, pen, line, headerColor, scale);
            pen.y += lineHeight;
        }

        // Gráfico: una barra por frame (el más nuevo a la derecha) y guías en 60 y 30 FPS
        const glm::vec2 graph = pen + glm::vec2(0.0f, pad * 0.5f);
        batch.rect(graph, graphSize, {0.0f, 0.0f, 0.0f, 0.5f});
        const float firstX = graph.x + float(kHistory - m_count) * scale;
        for (std::size_t i = 0; i < m_count; i++) {
            const float ms = sample(i);
            const float h = std::min(ms / kGraphMaxMs, 1.0f) * graphSize.y;
            batch.rect({firstX + float(i) * scale, graph.y + graphSize.y - h}, {scale, h}, barColor(ms));
        }
        for (const float guide : {kTarget60Ms, kTarget30Ms}) {
            const float y = graph.y + graphSize.y * (1.0f - guide / kGraphMaxMs);
            batch.rect({graph.x, std::round(y)}, {graphSize.x, std::max(1.0f, std::floor(scale))},
                       {1.0f, 1.0f, 1.0f, 0.35f});
        }
        pen.y = graph.y + graphSize.y + pad * 0.5f;

        for (const std::string& line : lines) {
            batch.text(font, atlasTexture, pen, line, {0.9f, 0.9f, 0.9f, 1.0f}, scale);
            pen.y += lineHeight;
        }
        return size;
    }

    glm::vec2 DebugOverlay::draw(Renderer2D& ui, const glm::vec2& pos, float scale) const {
        if (!m_visible) return glm::vec2(0.0f);
        const GpuFont& font = ui.builtinFont();
        return build(ui, font.font, &font.texture, pos, scale);
    }
}
