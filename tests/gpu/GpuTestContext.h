#pragma once

#include <mge/gpu/GpuDevice.h>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <doctest/doctest.h>

// Device de GPU para los tests (sin ventana: se dibuja en texturas y se leen los píxeles).
// - Usa el driver de video "offscreen" de SDL salvo que SDL_VIDEODRIVER diga otra cosa.
// - Sin GPU compatible el test se saltea con un aviso; con MGE_REQUIRE_GPU=1 (CI) falla.
// - Pide las capas de validación de Vulkan (modo debug). Los errores los imprime la capa (SDL no los
//   recibe); en CI la capa está configurada para cortar el proceso ante un error
//   (VK_KHRONOS_VALIDATION_DEBUG_ACTION=...BREAK), así que un uso incorrecto de la API o un recurso sin
//   liberar al destruir el device hace fallar el test. Con MGE_REQUIRE_VALIDATION=1 (CI) falla si las
//   capas no están activas. tests/gpu/ValidationProbe.cpp comprueba que ese corte funciona.
namespace mge_test {

    class GpuTestContext {
    public:
        GpuTestContext() {
            if (!std::getenv("SDL_VIDEODRIVER")) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
            s_validationActive = false;
            SDL_GetLogOutputFunction(&s_previousLog, &s_previousUserdata);
            SDL_SetLogOutputFunction(&captureLog, nullptr);
            m_previousPriority = SDL_GetLogPriority(SDL_LOG_CATEGORY_GPU);
            SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU,
                               SDL_LOG_PRIORITY_INFO); // Para ver "Validation layers enabled"
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return;
            m_videoInit = true;
            mge::GpuDeviceOptions options;
            options.debug = true; // Validación si las capas están instaladas
            m_device = mge::GpuDevice::create(options);
        }

        ~GpuTestContext() {
            m_device.reset();
            if (m_videoInit) SDL_QuitSubSystem(SDL_INIT_VIDEO);
            SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, m_previousPriority);
            SDL_SetLogOutputFunction(s_previousLog, s_previousUserdata);
        }

        // false = no hay GPU (o faltan las capas de validación exigidas): el test debe terminar
        bool available() const {
            if (!m_device) {
                if (envIs("MGE_REQUIRE_GPU", "1")) {
                    FAIL("No hay GPU compatible y MGE_REQUIRE_GPU=1: ", SDL_GetError());
                } else {
                    MESSAGE("Test salteado: no hay GPU compatible (", SDL_GetError(), ")");
                }
                return false;
            }
            if (!s_validationActive && envIs("MGE_REQUIRE_VALIDATION", "1")) {
                FAIL("MGE_REQUIRE_VALIDATION=1 pero las capas de validación de Vulkan no están activas");
                return false;
            }
            return true;
        }

        mge::GpuDevice& device() { return *m_device; }
        static bool validationActive() { return s_validationActive; }

        static bool envIs(const char* name, const char* value) {
            const char* env = std::getenv(name);
            return env && std::strcmp(env, value) == 0;
        }

    private:
        static void SDLCALL captureLog(void* /*userdata*/, int category, SDL_LogPriority priority,
                                       const char* message) {
            if (message && std::strstr(message, "Validation layers enabled")) s_validationActive = true;
            if (s_previousLog) s_previousLog(s_previousUserdata, category, priority, message);
        }

        inline static bool s_validationActive = false;
        inline static SDL_LogOutputFunction s_previousLog = nullptr;
        inline static void* s_previousUserdata = nullptr;

        bool m_videoInit = false;
        SDL_LogPriority m_previousPriority = SDL_LOG_PRIORITY_ERROR;
        std::unique_ptr<mge::GpuDevice> m_device;
    };
}
