#include "RenderTest.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstdlib>
#include <filesystem>

namespace mge_test {

    namespace {
        bool updateGoldens() {
            const char* env = std::getenv("MGE_UPDATE_GOLDEN");
            return env && std::string(env) == "1";
        }

        bool writePng(const std::filesystem::path& path, const Image& image) {
            std::filesystem::create_directories(path.parent_path());
            return stbi_write_png(path.string().c_str(), static_cast<int>(image.width),
                                  static_cast<int>(image.height), 4, image.rgba.data(),
                                  static_cast<int>(image.width * 4)) != 0;
        }
    }

    Image renderOffscreen(mge::GpuDevice& device, std::uint32_t width, std::uint32_t height,
                          const glm::vec4& clearColor, bool withDepth, const DrawFunction& draw) {
        mge::GpuTexture color = device.createRenderTarget(width, height);
        mge::GpuTexture depth;
        if (withDepth) depth = device.createDepthTarget(width, height);
        mge::CommandBuffer cmd = device.acquireCommandBuffer();
        {
            mge::ColorTarget target;
            target.texture = color.get();
            target.clearColor = clearColor;
            mge::DepthTarget depthTarget;
            depthTarget.texture = depth.get();
            mge::RenderPass pass = cmd.beginRenderPass(target, withDepth ? &depthTarget : nullptr);
            if (draw) draw(cmd, pass);
        }
        cmd.submitAndWait();
        return {width, height, device.downloadTexture(color, width, height)};
    }

    GoldenResult compareWithGolden(const std::string& name, const Image& image,
                                   const GoldenOptions& options) {
        GoldenResult result;
        const std::filesystem::path golden = std::filesystem::path(MGE_GOLDEN_DIR) / (name + ".png");
        const std::filesystem::path outDir = std::filesystem::path(MGE_RENDER_OUTPUT_DIR);

        if (updateGoldens()) {
            result.passed = writePng(golden, image);
            result.message = "MGE_UPDATE_GOLDEN=1: referencia reescrita en " + golden.string();
            return result;
        }

        int w = 0, h = 0, channels = 0;
        stbi_uc* data = stbi_load(golden.string().c_str(), &w, &h, &channels, 4);
        if (!data) {
            writePng(outDir / (name + ".png"), image);
            result.message = "No existe la referencia " + golden.string() +
                             " (generarla con MGE_UPDATE_GOLDEN=1; lo dibujado quedó en " + outDir.string() +
                             ")";
            return result;
        }
        if (static_cast<std::uint32_t>(w) != image.width || static_cast<std::uint32_t>(h) != image.height) {
            stbi_image_free(data);
            result.message = "Tamaño distinto de la referencia " + golden.string();
            return result;
        }

        Image diff{image.width, image.height, std::vector<std::uint8_t>(image.rgba.size(), 0)};
        std::size_t different = 0;
        const std::size_t pixels = std::size_t(w) * std::size_t(h);
        for (std::size_t i = 0; i < pixels; i++) {
            int maxDelta = 0;
            for (int c = 0; c < 4; c++)
                maxDelta = std::max(maxDelta, std::abs(int(image.rgba[i * 4 + c]) - int(data[i * 4 + c])));
            if (maxDelta > options.channelTolerance) different++;
            diff.rgba[i * 4 + 0] =
                static_cast<std::uint8_t>(std::min(255, maxDelta * 4)); // Rojo = diferencia
            diff.rgba[i * 4 + 3] = 255;
        }
        stbi_image_free(data);

        result.differentFraction = double(different) / double(pixels);
        result.passed = result.differentFraction <= options.maxDifferentPixels;
        if (!result.passed) {
            writePng(outDir / (name + ".png"), image);
            writePng(outDir / (name + "_diff.png"), diff);
            result.message = name + ": " + std::to_string(different) + " píxeles distintos (" +
                             std::to_string(result.differentFraction * 100.0) + " %); ver " + outDir.string();
        }
        return result;
    }
}
