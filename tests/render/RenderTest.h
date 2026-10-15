#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Tests de render: se dibuja una escena en una textura (sin ventana), se lee a la CPU y se compara con una
// imagen de referencia ("golden") en tests/render/golden/<nombre>.png.
//   - MGE_UPDATE_GOLDEN=1 reescribe las referencias con lo que se dibujó (revisarlas antes de commitear).
//   - Si no coincide, la imagen obtenida y la diferencia quedan en <build>/tests/render_output/.
// Las GPUs no rasterizan igual al píxel (bordes de triángulos): por eso hay tolerancia por canal y un
// máximo de píxeles distintos.
namespace mge_test {

    struct Image {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba; // Fila 0 arriba

        const std::uint8_t* pixel(std::uint32_t x, std::uint32_t y) const {
            return &rgba[(y * width + x) * 4];
        }
    };

    // Dibuja en un color target RGBA8 (+ depth si withDepth) y devuelve los píxeles
    using DrawFunction = std::function<void(mge::CommandBuffer& cmd, mge::RenderPass& pass)>;
    Image renderOffscreen(mge::GpuDevice& device, std::uint32_t width, std::uint32_t height,
                          const glm::vec4& clearColor, bool withDepth, const DrawFunction& draw);

    struct GoldenOptions {
        int channelTolerance = 8;         // Diferencia máxima por canal (0-255) para que un píxel "coincida"
        double maxDifferentPixels = 0.01; // Fracción de píxeles distintos permitida (1 %)
    };

    struct GoldenResult {
        bool passed = false;
        double differentFraction = 0.0;
        std::string message;
    };

    GoldenResult compareWithGolden(const std::string& name, const Image& image,
                                   const GoldenOptions& options = {});
}
