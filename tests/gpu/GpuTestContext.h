#pragma once

#include <mge/gpu/GpuDevice.h>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

// Device de GPU para los tests (sin ventana: se dibuja en texturas y se leen los píxeles).
// - Usa el driver de video "offscreen" de SDL salvo que SDL_VIDEODRIVER diga otra cosa.
// - Sin GPU compatible el test se saltea con un aviso; con MGE_REQUIRE_GPU=1 (CI) falla.
// - Junta los mensajes de validación de la GPU del log de SDL: un test limpio no tiene ninguno.
namespace mge_test {

    class GpuTestContext {
    public:
        GpuTestContext() {
            s_validationMessages.clear();
            if (!std::getenv("SDL_VIDEODRIVER")) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
            SDL_GetLogOutputFunction(&s_previousLog, &s_previousUserdata);
            SDL_SetLogOutputFunction(&captureLog, nullptr);
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return;
            m_videoInit = true;
            mge::GpuDeviceOptions options;
            options.debug = true; // Validación si las capas están instaladas
            m_device = mge::GpuDevice::create(options);
        }

        ~GpuTestContext() {
            m_device.reset();
            if (m_videoInit) SDL_QuitSubSystem(SDL_INIT_VIDEO);
            SDL_SetLogOutputFunction(s_previousLog, s_previousUserdata);
        }

        // false = no hay GPU: el test debe terminar (ya se reportó como aviso o como falla)
        bool available() const {
            if (m_device) return true;
            const char* require = std::getenv("MGE_REQUIRE_GPU");
            if (require && std::string(require) == "1") {
                FAIL("No hay GPU compatible y MGE_REQUIRE_GPU=1: ", SDL_GetError());
            } else {
                MESSAGE("Test salteado: no hay GPU compatible (", SDL_GetError(), ")");
            }
            return false;
        }

        mge::GpuDevice& device() { return *m_device; }

        // Mensajes de las capas de validación capturados desde que se creó el contexto
        static const std::vector<std::string>& validationMessages() { return s_validationMessages; }

    private:
        static void SDLCALL captureLog(void* /*userdata*/, int category, SDL_LogPriority priority,
                                       const char* message) {
            const std::string text = message ? message : "";
            if (text.find("Validation") != std::string::npos || text.find("VUID") != std::string::npos)
                s_validationMessages.push_back(text);
            if (s_previousLog) s_previousLog(s_previousUserdata, category, priority, message);
        }

        inline static std::vector<std::string> s_validationMessages;
        inline static SDL_LogOutputFunction s_previousLog = nullptr;
        inline static void* s_previousUserdata = nullptr;

        bool m_videoInit = false;
        std::unique_ptr<mge::GpuDevice> m_device;
    };
}
