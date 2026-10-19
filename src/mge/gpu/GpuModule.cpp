#include <mge/gpu/GpuModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/gpu/Swapchain.h>
#include <mge/platform/PlatformModule.h>

#include <cstdlib>
#include <string>

namespace mge {

    bool GpuModule::onInit(Application& app) {
        m_platform = app.getModule<PlatformModule>();
        if (!m_platform) {
            MGE_LOG_FATAL("Gpu", "GpuModule necesita un PlatformModule agregado antes (la ventana)");
            return false;
        }
        // MGE_GPU_DEBUG=0/1 pisa la opción (ej: validación en un build Release)
        GpuDeviceOptions options = m_options;
        if (const char* debug = std::getenv("MGE_GPU_DEBUG")) options.debug = std::string(debug) != "0";
        m_device = GpuDevice::create(options);
        if (!m_device) return false;
        if (!SDL_ClaimWindowForGPUDevice(m_device->get(), m_platform->getWindow().getSDLWindow())) {
            MGE_LOG_FATAL("Gpu", "La GPU no pudo tomar la ventana: ", SDL_GetError());
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
            MGE_LOG_WARN("Gpu", "La ventana no permite desactivar el vsync: se mantiene");
        if (!SDL_SetGPUSwapchainParameters(m_device->get(), window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
            MGE_LOG_ERROR("Gpu", "No se pudo cambiar el modo de presentación: ", SDL_GetError());
            return mode;
        }
        m_vsync = mode == SDL_GPU_PRESENTMODE_VSYNC;
        MGE_LOG_INFO("Gpu", "Presentación: ", presentModeName(mode));
        return mode;
    }

    void GpuModule::onFrameBegin(Application& app) {
        m_frame = {};
        m_cmd = m_device->acquireCommandBuffer();
        if (!m_cmd) return;
        SDL_GPUTexture* swapchain = nullptr;
        std::uint32_t width = 0, height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_cmd.get(), m_platform->getWindow().getSDLWindow(),
                                                   &swapchain, &width, &height)) {
            MGE_LOG_ERROR("Gpu", "No se pudo adquirir la imagen del swapchain: ", SDL_GetError());
            return;
        }
        // swapchain == nullptr es normal (ventana minimizada): el frame se saltea sin error
        m_frame = {&m_cmd, swapchain, width, height};
        if (swapchain) app.setViewport({width, height}); // Las cámaras (CameraSystem) siguen al tamaño real
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
            SDL_ReleaseWindowFromGPUDevice(m_device->get(), m_platform->getWindow().getSDLWindow());
            m_device.reset();
        }
    }

    SDL_GPUTextureFormat GpuModule::swapchainFormat() const {
        return SDL_GetGPUSwapchainTextureFormat(m_device->get(), m_platform->getWindow().getSDLWindow());
    }
}
