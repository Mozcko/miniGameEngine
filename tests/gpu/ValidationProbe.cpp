// Verifies that Vulkan validation errors reach test output, which CTest checks
// (FAIL_REGULAR_EXPRESSION "Validation Error" on all GPU tests): without this, incorrect API
// usage or unreleased resources would go unnoticed.
// Triggers a real error: a fragment shader that uses a uniform buffer, declared with 0 uniform
// buffers (SDL does not inspect SPIR-V; the validation layer catches it when creating the pipeline).
// The CTest test passes only if that VUID appears in output (PASS_REGULAR_EXPRESSION).
// 77 = skipped (no GPU or no validation layers).
#include <mge/gpu/GpuDevice.h>

#include <shaders/test_triangle_frag.h>
#include <shaders/test_triangle_vert.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
    bool validationActive = false; // Set by SDL log when creating the device

    void SDLCALL captureLog(void* /*userdata*/, int /*category*/, SDL_LogPriority /*priority*/,
                            const char* message) {
        if (message && std::strstr(message, "Validation layers enabled")) validationActive = true;
        std::fprintf(stderr, "%s\n", message ? message : "");
    }

    bool required(const char* name) {
        const char* env = std::getenv(name);
        return env && std::strcmp(env, "1") == 0;
    }
}

int main() {
    if (!std::getenv("SDL_VIDEODRIVER")) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    SDL_SetLogOutputFunction(&captureLog, nullptr);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return required("MGE_REQUIRE_GPU") ? 1 : 77;

    mge::GpuDeviceOptions options;
    options.debug = true;
    auto device = mge::GpuDevice::create(options);
    if (!device) return required("MGE_REQUIRE_GPU") ? 1 : 77;
    if (!validationActive) {
        std::printf("Validation layers not active\n");
        return required("MGE_REQUIRE_VALIDATION") ? 1 : 77;
    }

    // Intentional error: fragment shader uses a uniform buffer (set 3) but 0 are declared
    const mge::GpuShader vertex = device->createShader(mge::shaders::kTestTriangleVert);
    const mge::GpuShader fragment = device->createShader(mge::shaders::kTestTriangleFrag); // Missing resources
    mge::GraphicsPipelineDesc desc;
    desc.vertexShader = &vertex;
    desc.fragmentShader = &fragment;
    desc.gpuVertexInput = false;
    const mge::GpuGraphicsPipeline pipeline = device->createGraphicsPipeline(desc);
    std::fflush(stdout); // Layer writes to stdout: flush so CTest sees it before exit
    return 0;
}
