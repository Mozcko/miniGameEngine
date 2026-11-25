// Minimal SDL3 GPU example (Vulkan/Metal/D3D12): PlatformModule (window) + GpuModule (device and
// swapchain). A rotating triangle with GLSL shaders compiled to SPIR-V at build time and
// embedded (mge_add_shaders): the program reads no files. V toggles vsync (FPS is logged
// every second). Esc quits.
#include <mge/mge.h>

#include <shaders/triangle_frag.h>
#include <shaders/triangle_vert.h>

#include <cmath>
#include <cstdint>

namespace {
    // Matches the Params block in triangle.vert (std140: two consecutive floats)
    struct TriangleParams {
        float angle;
        float aspect;
    };

    class GpuTriangle : public mge::Application {
    public:
        GpuTriangle() : mge::Application(makeConfig()) {
            addModule<mge::PlatformModule>();
            addModule<mge::GpuModule>(); // After platform: requires the window
        }

    protected:
        bool onInit() override {
            m_platform = getModule<mge::PlatformModule>();
            m_gpu = getModule<mge::GpuModule>();
            m_platform->actions().bind("Quit", mge::Key::Escape);
            m_platform->actions().bind("ToggleVsync", mge::Key::V);

            mge::GpuDevice& device = m_gpu->device();
            mge::ShaderResources vertexResources;
            vertexResources.uniformBuffers = 1; // Params
            const mge::GpuShader vertex = device.createShader(mge::shaders::kTriangleVert, vertexResources);
            const mge::GpuShader fragment = device.createShader(mge::shaders::kTriangleFrag);

            mge::GraphicsPipelineDesc desc;
            desc.vertexShader = &vertex;
            desc.fragmentShader = &fragment;
            desc.colorFormat = m_gpu->swapchainFormat();
            desc.gpuVertexInput = false;           // Positions from gl_VertexIndex
            desc.cullMode = SDL_GPU_CULLMODE_NONE; // Visible from both sides while rotating
            m_pipeline = device.createGraphicsPipeline(desc);
            // Shaders can now be released: the pipeline already owns them
            if (!m_pipeline) return false;

            MGE_LOG_INFO("GpuTriangle", "GPU: ", device.driverName(), ". V: vsync, Esc: quit");
            return true;
        }

        void onUpdate(float dt) override {
            if (m_platform->wasActionPressed("Quit")) requestStop();
            if (m_platform->wasActionPressed("ToggleVsync")) m_gpu->setVsync(!m_gpu->vsync());
            m_phase += dt;

            // FPS every second (real time)
            m_fpsFrames++;
            m_fpsElapsed += time().unscaledDelta();
            if (m_fpsElapsed >= 1.0f) {
                MGE_LOG_INFO("GpuTriangle", "FPS: ", static_cast<int>(m_fpsFrames / m_fpsElapsed),
                             m_gpu->vsync() ? " (vsync)" : " (no vsync)");
                m_fpsFrames = 0;
                m_fpsElapsed = 0.0f;
            }
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame) return; // Window minimized: no image this frame

            mge::ColorTarget target;
            target.texture = frame->swapchain;
            target.clearColor = {0.1f + 0.05f * std::sin(m_phase), 0.1f, 0.15f, 1.0f};

            const TriangleParams params{m_phase, float(frame->width) / float(frame->height)};
            SDL_PushGPUVertexUniformData(frame->cmd->get(), 0, &params, sizeof(params));
            mge::RenderPass pass = frame->cmd->beginRenderPass(target); // Ends on scope exit
            SDL_BindGPUGraphicsPipeline(pass.get(), m_pipeline.get());
            SDL_DrawGPUPrimitives(pass.get(), 3, 1, 0, 0);
            m_presented++;
        }

        void onShutdown() override {
            m_pipeline.reset(); // Before the device (GpuModule shuts it down afterwards)
            MGE_LOG_INFO("GpuTriangle", m_presented, " frames presented out of ", getFrameCount());
            if (m_presented == 0) setExitCode(1); // Smoke test: at least one frame must be presented
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
        int m_fpsFrames = 0;
        float m_fpsElapsed = 0.0f;
        std::uint64_t m_presented = 0;
    };
}

mge::Application* mge::createApplication() {
    return new GpuTriangle();
}
