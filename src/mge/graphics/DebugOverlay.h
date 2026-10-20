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

    // Overlay de rendimiento (#43): FPS, gráfico de tiempos de frame y contadores con nombre. No sabe nada
    // del juego: el juego le pasa los tiempos y los contadores que quiera mostrar. Uso por frame:
    //   overlay.addFrame(time().unscaledDelta() * 1000, frame->acquireMs);
    //   overlay.setCounter("chunks", std::to_string(n));
    //   overlay.draw(ui);  // dentro de ui.begin ... ui.flush
    // La GPU de SDL no tiene consultas de tiempo (timestamps): se muestra el tiempo de CPU y la espera por la
    // imagen del swapchain (vsync / GPU ocupada), no el tiempo de GPU.
    class DebugOverlay {
    public:
        static constexpr std::size_t kHistory = 240; // Frames del gráfico (un píxel por frame)
        static constexpr float kGraphHeight = 48.0f;
        static constexpr float kGraphMaxMs = 50.0f; // Arriba del gráfico (las barras más altas se recortan)

        bool visible() const { return m_visible; }
        void setVisible(bool visible) { m_visible = visible; }
        void toggle() { m_visible = !m_visible; }

        // frameMs: duración total del frame; waitMs: la parte esperando la imagen del swapchain
        void addFrame(float frameMs, float waitMs = 0.0f);
        std::size_t sampleCount() const { return m_count; }
        // Del historial: FPS de los últimos ~0.5 s, promedio y máximo
        float fps() const;
        float averageMs() const;
        float maxMs() const;
        float averageWaitMs() const;

        // Línea "nombre: valor" (en el orden en que se agregaron; nombre vacío = solo el valor)
        void setCounter(std::string_view name, std::string value);
        void removeCounter(std::string_view name);
        const std::vector<std::pair<std::string, std::string>>& counters() const { return m_counters; }

        // Arma el panel en batch con la fuente dada (su atlas ya en la GPU como atlasTexture). Devuelve el
        // tamaño. Testeable sin GPU
        glm::vec2 build(Batch2D& batch, const Font& font, const Texture2D* atlasTexture,
                        const glm::vec2& pos = glm::vec2(8.0f), float scale = 1.0f) const;
        // Con la fuente embebida del renderer; no dibuja nada si no está visible
        glm::vec2 draw(Renderer2D& ui, const glm::vec2& pos = glm::vec2(8.0f), float scale = 1.0f) const;

    private:
        // i = 0 → el frame más viejo del historial
        float sample(std::size_t i) const { return m_frameMs[(m_head + kHistory - m_count + i) % kHistory]; }

        std::array<float, kHistory> m_frameMs{};
        std::array<float, kHistory> m_waitMs{};
        std::size_t m_head = 0; // Próxima posición a escribir
        std::size_t m_count = 0;
        std::vector<std::pair<std::string, std::string>> m_counters;
        bool m_visible = true;
    };
}
