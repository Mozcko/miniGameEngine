#pragma once

#include <mge/gpu/GpuDevice.h>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <doctest/doctest.h>

// GPU device for tests (headless: renders into textures and reads pixels back).
// - Uses SDL's "offscreen" video driver unless SDL_VIDEODRIVER specifies otherwise.
// - Without a compatible GPU, the test is skipped with a notice; with MGE_REQUIRE_GPU=1 (CI), it fails.
// - Requests Vulkan validation layers (debug mode). Errors are printed by the layer to stdout/stderr
//   (SDL does not receive them) and CTest fails the test if "Validation Error" appears
//   (FAIL_REGULAR_EXPRESSION): incorrect API usage or unreleased resources when destroying the device will
//   fail the test. With MGE_REQUIRE_VALIDATION=1 (CI), it fails if the layers are not active.
//   tests/gpu/ValidationProbe.cpp verifies that validation errors reach the output.
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
                               SDL_LOG_PRIORITY_INFO); // To see "Validation layers enabled"
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return;
            m_videoInit = true;
            mge::GpuDeviceOptions options;
            options.debug = true; // Validation if layers are installed
            m_device = mge::GpuDevice::create(options);
        }

        ~GpuTestContext() {
            m_device.reset();
            if (m_videoInit) SDL_QuitSubSystem(SDL_INIT_VIDEO);
            SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, m_previousPriority);
            SDL_SetLogOutputFunction(s_previousLog, s_previousUserdata);
        }

        // false = no GPU available (or required validation layers missing): the test should return
        bool available() const {
            if (!m_device) {
                if (envIs("MGE_REQUIRE_GPU", "1")) {
                    FAIL("No compatible GPU found and MGE_REQUIRE_GPU=1: ", SDL_GetError());
                } else {
                    MESSAGE("Test skipped: no compatible GPU found (", SDL_GetError(), ")");
                }
                return false;
            }
            if (!s_validationActive && envIs("MGE_REQUIRE_VALIDATION", "1")) {
                FAIL("MGE_REQUIRE_VALIDATION=1 but Vulkan validation layers are not active");
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
