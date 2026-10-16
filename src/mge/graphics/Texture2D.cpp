#include <mge/graphics/Texture2D.h>

#include <mge/core/Log.h>

#include <cstring>

// stb_image con funciones static: no choca con otra copia de stb_image en el mismo programa (ej: tests)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace mge {

    namespace {
        // Filas invertidas (la GPU guarda la imagen al revés: ver Texture2D.h)
        std::vector<std::uint8_t> flipRows(const std::uint8_t* rgba, std::uint32_t width,
                                           std::uint32_t height) {
            const std::size_t row = std::size_t(width) * 4;
            std::vector<std::uint8_t> flipped(row * height);
            for (std::uint32_t y = 0; y < height; y++)
                std::memcpy(&flipped[(height - 1 - y) * row], &rgba[y * row], row);
            return flipped;
        }
    }

    Texture2D Texture2D::fromPixels(GpuDevice& device, std::uint32_t width, std::uint32_t height,
                                    const std::uint8_t* rgba, bool keepCpuCopy) {
        Texture2D texture;
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        texture.m_texture = device.createTexture(info);
        if (!texture.m_texture) return {};
        texture.m_width = width;
        texture.m_height = height;
        const std::vector<std::uint8_t> flipped = flipRows(rgba, width, height);
        if (!device.uploadToTexture(texture.m_texture, 0, 0, width, height, flipped.data())) return {};
        if (keepCpuCopy) texture.m_original.assign(rgba, rgba + std::size_t(width) * height * 4);
        return texture;
    }

    Texture2D Texture2D::fromFile(GpuDevice& device, const std::filesystem::path& path, bool keepCpuCopy) {
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4); // Siempre RGBA
        if (!pixels) {
            MGE_LOG_ERROR("Texture", "No se pudo leer ", path.string(), ": ", stbi_failure_reason());
            return {};
        }
        Texture2D texture = fromPixels(device, static_cast<std::uint32_t>(width),
                                       static_cast<std::uint32_t>(height), pixels, keepCpuCopy);
        stbi_image_free(pixels);
        if (texture) MGE_LOG_DEBUG("Texture", path.filename().string(), ": ", width, "x", height);
        return texture;
    }

    Texture2D Texture2D::white(GpuDevice& device) {
        const std::uint8_t pixel[4] = {255, 255, 255, 255};
        return fromPixels(device, 1, 1, pixel);
    }

    bool Texture2D::updateRegion(GpuDevice& device, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                                 std::uint32_t height, const std::uint8_t* rgba) {
        if (!m_texture || x + width > m_width || y + height > m_height) return false;
        const std::vector<std::uint8_t> flipped = flipRows(rgba, width, height);
        // En la GPU la región está espejada en vertical
        return device.uploadToTexture(m_texture, x, m_height - y - height, width, height, flipped.data());
    }

    bool Texture2D::copyFromOriginal(GpuDevice& device, std::uint32_t srcX, std::uint32_t srcY,
                                     std::uint32_t width, std::uint32_t height, std::uint32_t dstX,
                                     std::uint32_t dstY) {
        if (m_original.empty() || srcX + width > m_width || srcY + height > m_height) return false;
        std::vector<std::uint8_t> region(std::size_t(width) * height * 4);
        for (std::uint32_t row = 0; row < height; row++)
            std::memcpy(&region[std::size_t(row) * width * 4],
                        &m_original[((srcY + row) * m_width + srcX) * 4], std::size_t(width) * 4);
        return updateRegion(device, dstX, dstY, width, height, region.data());
    }
}
