// Prueba que las capas de validación de Vulkan cortan el proceso ante un error (lo que hace que los
// tests de GPU fallen si usan mal la API o dejan recursos sin liberar).
// Provoca un error real: un shader de fragmentos que usa un uniform buffer, declarado con 0 uniform
// buffers (SDL no lee el SPIR-V; la capa sí lo detecta al crear el pipeline).
// Salida: 0 = la capa cortó (SIGTRAP), 1 = no cortó (o faltan requisitos exigidos), 77 = se saltea.
#include <mge/gpu/GpuDevice.h>

#include <shaders/test_triangle_frag.h>
#include <shaders/test_triangle_vert.h>

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {
    bool validationActive = false; // Lo marca el log de SDL al crear el device

    void SDLCALL captureLog(void* /*userdata*/, int /*category*/, SDL_LogPriority /*priority*/,
                            const char* message) {
        if (message && std::strstr(message, "Validation layers enabled")) validationActive = true;
        std::fprintf(stderr, "%s\n", message ? message : "");
    }

    void onTrap(int /*signal*/) {
        const char text[] = "Validación: la capa cortó el proceso ante el error, como se espera\n";
        (void)!write(STDOUT_FILENO, text, sizeof(text) - 1);
        _exit(0);
    }

    bool required(const char* name) {
        const char* env = std::getenv(name);
        return env && std::strcmp(env, "1") == 0;
    }
}

int main() {
    // Los errores de validación: imprimir y cortar (SIGTRAP)
    setenv("VK_KHRONOS_VALIDATION_REPORT_FLAGS", "error", 1);
    setenv("VK_KHRONOS_VALIDATION_DEBUG_ACTION", "VK_DBG_LAYER_ACTION_LOG_MSG,VK_DBG_LAYER_ACTION_BREAK", 1);
    std::signal(SIGTRAP, onTrap);

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

    std::printf("La capa de validación NO cortó el proceso ante un error\n");
    return 1;
}
