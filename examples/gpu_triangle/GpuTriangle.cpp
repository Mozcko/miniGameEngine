// Ejemplo mínimo de SDL3 GPU (Vulkan/Metal/D3D12) sin OpenGL: PlatformModule (ventana sin contexto GL)
// + GpuModule (device y swapchain). Un triángulo que gira, con shaders GLSL compilados a SPIR-V en el
// build y embebidos (mge_add_shaders): el programa no lee ningún archivo. Esc sale.
#include <mge/mge.h>

#include <shaders/triangle_frag.h>
#include <shaders/triangle_vert.h>

#include <cmath>
#include <cstdint>

namespace {
    // Igual que el bloque Params de triangle.vert (std140: dos floats seguidos)
    struct TriangleParams {
        float angle;
        float aspect;
    };

    class GpuTriangle : public mge::Application {
    public:
        GpuTriangle() : mge::Application(makeConfig()) {
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

            mge::GpuDevice& device = m_gpu->device();
            mge::ShaderResources vertexResources;
            vertexResources.uniformBuffers = 1; // Params
            const mge::GpuShader vertex = device.createShader(mge::shaders::kTriangleVert, vertexResources);
            const mge::GpuShader fragment = device.createShader(mge::shaders::kTriangleFrag);

            mge::GraphicsPipelineDesc desc;
            desc.vertexShader = &vertex;
            desc.fragmentShader = &fragment;
            desc.colorFormat = m_gpu->swapchainFormat();
            desc.gpuVertexInput = false;           // Posiciones desde gl_VertexIndex
            desc.cullMode = SDL_GPU_CULLMODE_NONE; // Se ve de los dos lados al girar
            m_pipeline = device.createGraphicsPipeline(desc);
            // Los shaders se pueden liberar: el pipeline ya los tiene
            if (!m_pipeline) return false;

            MGE_LOG_INFO("GpuTriangle", "GPU: ", device.driverName(), ". Esc: salir");
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
            target.clearColor = {0.1f + 0.05f * std::sin(m_phase), 0.1f, 0.15f, 1.0f};

            const TriangleParams params{m_phase, float(frame->width) / float(frame->height)};
            SDL_PushGPUVertexUniformData(frame->cmd->get(), 0, &params, sizeof(params));
            mge::RenderPass pass = frame->cmd->beginRenderPass(target); // Se cierra al salir del scope
            SDL_BindGPUGraphicsPipeline(pass.get(), m_pipeline.get());
            SDL_DrawGPUPrimitives(pass.get(), 3, 1, 0, 0);
            m_presented++;
        }

        void onShutdown() override {
            m_pipeline.reset(); // Antes que el device (lo cierra el GpuModule después)
            MGE_LOG_INFO("GpuTriangle", m_presented, " frames presentados de ", getFrameCount());
            if (m_presented == 0) setExitCode(1); // Smoke test: tiene que haber llegado alguna imagen
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "GPU triangle";
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        mge::GpuGraphicsPipeline m_pipeline;
        float m_phase = 0.0f;
        std::uint64_t m_presented = 0;
    };
}

mge::Application* mge::createApplication() {
    return new GpuTriangle();
}
