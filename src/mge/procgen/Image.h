#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Imagen RGBA8 en la CPU (fila 0 arriba), para generar texturas por código (#34). Se sube a la GPU con
    // Texture2D::fromPixels(device, w, h, image.data()). Headless: sin dependencias.
    class Image {
    public:
        Image() = default;
        Image(std::uint32_t width, std::uint32_t height, const glm::vec4& fill = glm::vec4(0.0f));

        std::uint32_t width() const { return m_width; }
        std::uint32_t height() const { return m_height; }
        bool empty() const { return m_pixels.empty(); }
        const std::uint8_t* data() const { return m_pixels.data(); }
        const std::vector<std::uint8_t>& pixels() const { return m_pixels; }

        // Color en [0, 1] (se recorta). Fuera de la imagen: get = transparente, set = se ignora
        glm::vec4 get(int x, int y) const;
        void set(int x, int y, const glm::vec4& color);
        void fill(const glm::vec4& color);

    private:
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        std::vector<std::uint8_t> m_pixels;
    };

    // PNG sin comprimir (bloques "stored" de deflate): válido para cualquier visor, sin librerías.
    // Para inspeccionar texturas generadas. false si no se pudo escribir.
    bool writePng(const Image& image, const std::filesystem::path& path);
}
