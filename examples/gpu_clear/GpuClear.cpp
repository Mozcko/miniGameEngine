// Ejemplo mínimo de SDL3 GPU (Vulkan/Metal/D3D12) sin OpenGL: PlatformModule (ventana sin contexto GL)
// + GpuModule (device y swapchain). Cada frame abre un render pass que limpia la imagen con un color.
// Esc sale.
#include <mge/mge.h>

#include <cmath>
#include <cstdint>

namespace {
    class GpuClear : public mge::Application {
    public:
        GpuClear() : mge::Application(makeConfig()) {
            mge::PlatformOptions platform;
            platform.openGl = false; // La ventana es de la GPU de SDL
            addModule<mge::PlatformModule>(platform);
            addModule<mge::GpuModule>(); // Después de la plataforma: necesita la ventana
        }

    protected:
        bool onInit() override {
            m_platform = getModule<mge::PlatformModule>();
            m_gpu = getModule<mge::GpuModule>();
            m_platform->actions().bind("Quit", mge::Key::Escape);
            MGE_LOG_INFO("GpuClear", "GPU: ", m_gpu->device().driverName(), ". Esc: salir");
            return true;
        }

        void onUpdate(float dt) override {
            if (m_platform->wasActionPressed("Quit")) requestStop();
            m_phase += dt;
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame) return; // Ventana minimizada: no hay imagen este frame

            mge::ColorTarget target;
            target.texture = frame->swapchain;
            target.clearColor = {0.5f + 0.4f * std::sin(m_phase), 0.25f, 0.5f + 0.4f * std::cos(m_phase),
                                 1.0f};
            mge::RenderPass pass = frame->cmd->beginRenderPass(target); // Se cierra al salir del scope
            m_presented++;
        }

        void onShutdown() override {
            MGE_LOG_INFO("GpuClear", m_presented, " frames presentados de ", getFrameCount());
            if (m_presented == 0) setExitCode(1); // Smoke test: tiene que haber llegado alguna imagen
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "GPU clear";
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        float m_phase = 0.0f;
        std::uint64_t m_presented = 0;
    };
}

mge::Application* mge::createApplication() {
    return new GpuClear();
}
