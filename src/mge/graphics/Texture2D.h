#pragma once

#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mge {

    // 2D RGBA8 texture on the GPU. Two paths (ADR 0004): from a file (PNG, JPG... via stb_image) or
    // from procedurally generated pixels.
    //
    // Engine UV convention (same as the OpenGL renderer): (0, 0) is the BOTTOM-left corner of
    // the image and v grows upward (used by MeshData, primitives, and the mge_voxel atlas). To match this,
    // the image is stored vertically flipped on the GPU; pixel coordinates in this API are image coordinates
    // (row 0 at top), and the flip is internal.
    class Texture2D {
    public:
        Texture2D() = default;

        // rgba: width x height x 4 bytes, row 0 at top. keepCpuCopy keeps the original pixels
        // (for copyFromOriginal: tile animations)
        static Texture2D fromPixels(GpuDevice& device, std::uint32_t width, std::uint32_t height,
                                    const std::uint8_t* rgba, bool keepCpuCopy = false);
        // Invalid if the file could not be read (reason is logged).
        // path: relative to the assets root (resolved with mge::assetPath) or absolute.
        static Texture2D fromFile(GpuDevice& device, const std::filesystem::path& path,
                                  bool keepCpuCopy = false);
        // Decodes an encoded image (PNG, JPG, etc.) from memory via stb_image into an RGBA8 texture.
        // Returns an invalid Texture2D on failure (reason is logged).
        static Texture2D fromMemory(GpuDevice& device, const std::uint8_t* bytes, std::size_t size,
                                    bool keepCpuCopy = false);
        // 1x1 white: for drawing untextured meshes with the same shader
        static Texture2D white(GpuDevice& device);

        // Replaces a region (image coordinates, row 0 at top) with RGBA8 pixels
        bool updateRegion(GpuDevice& device, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                          std::uint32_t height, const std::uint8_t* rgba);
        // Copies a region from the ORIGINAL image (keepCpuCopy) to another location in the texture (animated
        // tiles)
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
        std::vector<std::uint8_t> m_original; // Row 0 at top (empty without keepCpuCopy)
    };
}
