#include <mge/gpu/GpuModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/gpu/Swapchain.h>
#include <mge/platform/PlatformModule.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace mge {

    bool GpuModule::onInit(Application& app) {
        m_platform = app.getModule<PlatformModule>();
        if (!m_platform) {
            MGE_LOG_FATAL("Gpu", "GpuModule requires a PlatformModule added before it (the window)");
            return false;
        }
        // MGE_GPU_DEBUG=0/1 overrides the option (e.g. validation in a Release build)
        GpuDeviceOptions options = m_options;
        if (const char* debug = std::getenv("MGE_GPU_DEBUG")) options.debug = std::string(debug) != "0";
        m_device = GpuDevice::create(options);
        if (!m_device) return false;
        if (!SDL_ClaimWindowForGPUDevice(m_device->get(), m_platform->getWindow().getSDLWindow())) {
            MGE_LOG_FATAL("Gpu", "GPU failed to claim the window: ", SDL_GetError());
            m_device.reset();
            return false;
        }
        setVsync(app.getConfig().vsync);
        return true;
    }

    SDL_GPUPresentMode GpuModule::setVsync(bool vsync) {
        SDL_Window* window = m_platform->getWindow().getSDLWindow();
        const SDL_GPUPresentMode mode = choosePresentMode(vsync, [&](SDL_GPUPresentMode candidate) {
            return SDL_WindowSupportsGPUPresentMode(m_device->get(), window, candidate);
        });
        if (!vsync && mode == SDL_GPU_PRESENTMODE_VSYNC)
            MGE_LOG_WARN("Gpu", "Window does not support disabling vsync: keeping it enabled");
        if (!SDL_SetGPUSwapchainParameters(m_device->get(), window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
            MGE_LOG_ERROR("Gpu", "Failed to change present mode: ", SDL_GetError());
            return mode;
        }
        m_vsync = mode == SDL_GPU_PRESENTMODE_VSYNC;
        MGE_LOG_INFO("Gpu", "Present mode: ", presentModeName(mode));
        return mode;
    }

    void GpuModule::onFrameBegin(Application& app) {
        m_frame = {};
        m_cmd = m_device->acquireCommandBuffer();
        if (!m_cmd) return;
        SDL_GPUTexture* swapchain = nullptr;
        std::uint32_t width = 0, height = 0;
        const auto waitStart = std::chrono::steady_clock::now();
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_cmd.get(), m_platform->getWindow().getSDLWindow(),
                                                   &swapchain, &width, &height)) {
            MGE_LOG_ERROR("Gpu", "Failed to acquire swapchain texture: ", SDL_GetError());
            return;
        }
        const float acquireMs =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - waitStart).count();
        // swapchain == nullptr is normal (minimized window): skip the frame without error
        const float scale = SDL_GetWindowDisplayScale(m_platform->getWindow().getSDLWindow());
        m_frame = {&m_cmd, swapchain, width, height, scale > 0.0f ? scale : 1.0f, acquireMs};
        if (swapchain) app.setViewport({width, height}); // Cameras (CameraSystem) follow the actual size
    }

    void GpuModule::onFrameEnd(Application& /*app*/) {
        m_cmd.submit();
        m_frame = {};
    }

    void GpuModule::onShutdown(Application& /*app*/) {
        m_cmd.cancel();
        m_frame = {};
        if (m_device) {
            m_device->waitIdle();
            m_device->tracker().releaseAll();
            SDL_ReleaseWindowFromGPUDevice(m_device->get(), m_platform->getWindow().getSDLWindow());
            m_device.reset();
        }
    }

    SDL_GPUTextureFormat GpuModule::swapchainFormat() const {
        return SDL_GetGPUSwapchainTextureFormat(m_device->get(), m_platform->getWindow().getSDLWindow());
    }
}
