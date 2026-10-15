#include <mge/gpu/GpuModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/platform/PlatformModule.h>

namespace mge {

    bool GpuModule::onInit(Application& app) {
        m_platform = app.getModule<PlatformModule>();
        if (!m_platform) {
            MGE_LOG_FATAL("Gpu", "GpuModule necesita un PlatformModule agregado antes (la ventana)");
            return false;
        }
        if (m_platform->hasGlContext()) {
            MGE_LOG_FATAL("Gpu", "La ventana tiene un contexto OpenGL: crear el PlatformModule con "
                                 "PlatformOptions::openGl = false");
            return false;
        }
        m_device = GpuDevice::create(m_options);
        if (!m_device) return false;
        if (!SDL_ClaimWindowForGPUDevice(m_device->get(), m_platform->getWindow().getSDLWindow())) {
            MGE_LOG_FATAL("Gpu", "La GPU no pudo tomar la ventana: ", SDL_GetError());
            m_device.reset();
            return false;
        }
        return true;
    }

    void GpuModule::onFrameBegin(Application& /*app*/) {
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
