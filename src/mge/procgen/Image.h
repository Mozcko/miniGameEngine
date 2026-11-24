#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // RGBA8 CPU image (row 0 at the top), for procedural textures (#34). Uploaded to the GPU via
    // Texture2D::fromPixels(device, w, h, image.data()). Headless: no dependencies.
    class Image {
    public:
        Image() = default;
        Image(std::uint32_t width, std::uint32_t height, const glm::vec4& fill = glm::vec4(0.0f));

        std::uint32_t width() const { return m_width; }
        std::uint32_t height() const { return m_height; }
        bool empty() const { return m_pixels.empty(); }
        const std::uint8_t* data() const { return m_pixels.data(); }
        const std::vector<std::uint8_t>& pixels() const { return m_pixels; }

        // Color in [0, 1] (clamped). Out of bounds: get = transparent, set = ignored
        glm::vec4 get(int x, int y) const;
        void set(int x, int y, const glm::vec4& color);
        void fill(const glm::vec4& color);

    private:
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        std::vector<std::uint8_t> m_pixels;
    };

    // Uncompressed PNG (deflate "stored" blocks): valid in any viewer, dependency-free.
    // For inspecting generated textures. Returns false if writing failed.
    bool writePng(const Image& image, const std::filesystem::path& path);
}
