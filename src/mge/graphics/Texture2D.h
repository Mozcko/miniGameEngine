#pragma once

#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mge {

    // Textura 2D RGBA8 en la GPU. Dos caminos (ADR 0004): desde un archivo (PNG, JPG… con stb_image) o
    // desde píxeles generados por código.
    //
    // Convención de UV del motor (igual que el renderer OpenGL): (0, 0) es la esquina INFERIOR izquierda de
    // la imagen y v crece hacia arriba (lo usan MeshData, las primitivas y el atlas de mge_voxel). Para eso
    // la imagen se guarda en la GPU invertida verticalmente; las coordenadas de píxel de esta API son las de
    // la imagen (fila 0 arriba), la inversión es interna.
    class Texture2D {
    public:
        Texture2D() = default;

        // rgba: width × height × 4 bytes, fila 0 arriba. keepCpuCopy guarda los píxeles originales
        // (para copyFromOriginal: animaciones de tiles)
        static Texture2D fromPixels(GpuDevice& device, std::uint32_t width, std::uint32_t height,
                                    const std::uint8_t* rgba, bool keepCpuCopy = false);
        // Inválida si no se pudo leer el archivo (el motivo queda en el log)
        static Texture2D fromFile(GpuDevice& device, const std::filesystem::path& path,
                                  bool keepCpuCopy = false);
        // 1×1 blanca: para dibujar mallas sin textura con el mismo shader
        static Texture2D white(GpuDevice& device);

        // Reemplaza una región (coordenadas de la imagen, fila 0 arriba) con píxeles RGBA8
        bool updateRegion(GpuDevice& device, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                          std::uint32_t height, const std::uint8_t* rgba);
        // Copia una región de la imagen ORIGINAL (keepCpuCopy) a otro lugar de la textura (tiles animados)
        bool copyFromOriginal(GpuDevice& device, std::uint32_t srcX, std::uint32_t srcY, std::uint32_t width,
                              std::uint32_t height, std::uint32_t dstX, std::uint32_t dstY);

        const GpuTexture& gpu() const { return m_texture; }
        std::uint32_t width() const { return m_width; }
        std::uint32_t height() const { return m_height; }
        const std::vector<std::uint8_t>& originalPixels() const { return m_original; }
        explicit operator bool() const { return static_cast<bool>(m_texture); }

    private:
        GpuTexture m_texture;
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        std::vector<std::uint8_t> m_original; // Fila 0 arriba (vacío sin keepCpuCopy)
    };
}
