// Prueba que los errores de validación de Vulkan llegan a la salida del test, que es lo que CTest revisa
// (FAIL_REGULAR_EXPRESSION "Validation Error" en todos los tests de GPU): sin esto, un uso incorrecto de
// la API o un recurso sin liberar pasarían sin que nadie lo note.
// Provoca un error real: un shader de fragmentos que usa un uniform buffer, declarado con 0 uniform
// buffers (SDL no lee el SPIR-V; la capa sí lo detecta al crear el pipeline). El test de CTest pasa solo
// si en la salida aparece ese VUID (PASS_REGULAR_EXPRESSION). 77 = se saltea (sin GPU o sin capas).
#include <mge/gpu/GpuDevice.h>

#include <shaders/test_triangle_frag.h>
#include <shaders/test_triangle_vert.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
    bool validationActive = false; // Lo marca el log de SDL al crear el device

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
        std::printf("Capas de validación no activas\n");
        return required("MGE_REQUIRE_VALIDATION") ? 1 : 77;
    }

    // Error a propósito: el fragment shader usa un uniform buffer (set 3) pero se declaran 0
    const mge::GpuShader vertex = device->createShader(mge::shaders::kTestTriangleVert);
    const mge::GpuShader fragment = device->createShader(mge::shaders::kTestTriangleFrag); // Faltan recursos
    mge::GraphicsPipelineDesc desc;
    desc.vertexShader = &vertex;
    desc.fragmentShader = &fragment;
    desc.gpuVertexInput = false;
    const mge::GpuGraphicsPipeline pipeline = device->createGraphicsPipeline(desc);
    std::fflush(stdout); // La capa escribe en stdout: que CTest lo vea antes de salir
    return 0;
}
