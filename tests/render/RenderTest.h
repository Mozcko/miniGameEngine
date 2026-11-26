#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Render tests: renders a scene into an offscreen texture (no window), reads it back to the CPU, and compares
// it against a reference ("golden") image at tests/render/golden/<name>.png.
//   - MGE_UPDATE_GOLDEN=1 overwrites the reference images with what was rendered (review before committing).
//   - On mismatch, the rendered image and diff image are saved to <build>/tests/render_output/.
// GPUs do not rasterize identically to the pixel (triangle edges): hence a per-channel tolerance and a
// maximum fraction of differing pixels.
namespace mge_test {

    struct Image {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba; // Row 0 at top

        const std::uint8_t* pixel(std::uint32_t x, std::uint32_t y) const {
            return &rgba[(y * width + x) * 4];
        }
    };

    // Renders into an RGBA8 color target (+ depth if withDepth) and returns the pixels
    using DrawFunction = std::function<void(mge::CommandBuffer& cmd, mge::RenderPass& pass)>;
    Image renderOffscreen(mge::GpuDevice& device, std::uint32_t width, std::uint32_t height,
                          const glm::vec4& clearColor, bool withDepth, const DrawFunction& draw);

    struct GoldenOptions {
        int channelTolerance = 8;         // Max difference per channel (0-255) for a pixel to "match"
        double maxDifferentPixels = 0.01; // Allowed fraction of differing pixels (1%)
    };

    struct GoldenResult {
        bool passed = false;
        double differentFraction = 0.0;
        std::string message;
    };

    GoldenResult compareWithGolden(const std::string& name, const Image& image,
                                   const GoldenOptions& options = {});
}
