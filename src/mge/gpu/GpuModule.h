#pragma once

#include <mge/core/Module.h>
#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <memory>

namespace mge {

    class PlatformModule;

    // Lo que el juego necesita para dibujar el frame actual
    struct GpuFrame {
        CommandBuffer* cmd = nullptr;        // Se envía solo al final del frame
        SDL_GPUTexture* swapchain = nullptr; // La imagen que se presenta en la ventana
        std::uint32_t width = 0;             // Tamaño real del swapchain (cambia al redimensionar)
        std::uint32_t height = 0;
        // Escala de la pantalla (SDL_GetWindowDisplayScale: DPI y preferencia del usuario). Para la UI
        // (Renderer2D::begin); con la fuente 8x8 conviene redondearla a un entero
        float displayScale = 1.0f;
        // Milisegundos que este frame esperó la imagen del swapchain (vsync o GPU ocupada): el resto del
        // frame es CPU. Para el overlay de rendimiento (#43)
        float acquireMs = 0.0f;
    };

    // Módulo de GPU: crea el device (SDL3 GPU), toma la ventana del PlatformModule y en cada frame
    // adquiere un command buffer + la imagen del swapchain, y los envía al terminar el frame.
    // Agregarlo DESPUÉS del PlatformModule: se inicia después (la ventana ya existe) y se cierra antes.
    class GpuModule : public Module {
    public:
        explicit GpuModule(GpuDeviceOptions options = {}) : m_options(options) {}

        const char* name() const override { return "Gpu"; }

        bool onInit(Application& app) override;
        void onFrameBegin(Application& app) override;
        void onFrameEnd(Application& app) override;
        void onShutdown(Application& app) override;

        // Válido entre onInit y onShutdown
        GpuDevice& device() { return *m_device; }
        // nullptr si este frame no hay imagen (ventana minimizada, o fuera de onUpdate/onRender)
        const GpuFrame* frame() const { return m_frame.swapchain ? &m_frame : nullptr; }
        SDL_GPUTextureFormat swapchainFormat() const;

        // Vsync en caliente (el valor inicial sale de ApplicationConfig::vsync). Sin vsync usa MAILBOX o
        // IMMEDIATE si la ventana los soporta (choosePresentMode); devuelve el modo que quedó.
        SDL_GPUPresentMode setVsync(bool vsync);
        bool vsync() const { return m_vsync; }

    private:
        GpuDeviceOptions m_options;
        std::unique_ptr<GpuDevice> m_device;
        PlatformModule* m_platform = nullptr;
        CommandBuffer m_cmd;
        GpuFrame m_frame;
        bool m_vsync = true;
    };
}
