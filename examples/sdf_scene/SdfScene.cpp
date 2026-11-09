// Ejemplo de SDF raymarching (#36): formas SDF (blobs suaves, toro retorcido,
// columnas repetidas) intersectando una malla de plano de suelo y un cubo rasterizados.
#include <mge/mge.h>

#include <shaders/scene_frag.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <memory>

namespace {

    class SdfSceneApp : public mge::Application {
    public:
        SdfSceneApp() : mge::Application(makeConfig()), m_fly(glm::vec3(0.0f, 4.0f, 10.0f), -90.0f, -15.0f) {
            addModule<mge::PlatformModule>();
            addModule<mge::GpuModule>();
        }

    protected:
        bool onInit() override {
            m_platform = getModule<mge::PlatformModule>();
            m_gpu = getModule<mge::GpuModule>();
            if (!m_platform || !m_gpu) return false;

            m_platform->actions().bind("Quit", mge::Key::Escape);

            mge::GpuDevice& device = m_gpu->device();
            m_renderer = std::make_unique<mge::MeshRenderer>(device, m_gpu->swapchainFormat());
            m_sdfRenderer = std::make_unique<mge::SdfRenderer>(
                device, m_gpu->swapchainFormat(), device.depthFormat(), mge::shaders::kSceneFrag);

            if (!m_renderer || !m_sdfRenderer || !m_sdfRenderer->valid()) {
                MGE_LOG_ERROR("SdfScene", "No se pudo inicializar los renderizadores");
                return false;
            }

            // Mallas rasterizadas: plano de suelo y cubo procedural
            const mge::MeshData groundData =
                mge::primitives::plane(50.0f, 50.0f, 10, 10, glm::vec4(0.35f, 0.40f, 0.45f, 1.0f));
            const mge::MeshData cubeData =
                mge::primitives::box(glm::vec3(2.0f), glm::vec4(0.85f, 0.75f, 0.25f, 1.0f));

            m_groundMesh = device.uploadMesh(groundData);
            m_cubeMesh = device.uploadMesh(cubeData);

            m_lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
            m_lighting.sunColor = glm::vec3(0.9f, 0.85f, 0.8f);
            m_lighting.skyAmbient = glm::vec3(0.45f, 0.50f, 0.60f);
            m_lighting.groundAmbient = glm::vec3(0.25f, 0.22f, 0.20f);
            m_lighting.fog = true;
            m_lighting.fogColor = glm::vec3(0.5f, 0.7f, 1.0f);
            m_lighting.fogStart = 40.0f;
            m_lighting.fogEnd = 120.0f;

            MGE_LOG_INFO("SdfScene", "SDF Scene iniciada. WASD: mover, raton: mirar, Esc: salir");
            return true;
        }

        void onUpdate(float dt) override {
            if (!m_platform) return;
            const mge::InputState& in = m_platform->input();
            if (in.isDown(mge::Key::Escape)) {
                requestStop();
                return;
            }

            m_time += dt;

            // Actualización de la cámara fly
            mge::CameraInput camInput;
            if (in.isDown(mge::Key::W)) camInput.moveForward += 1.0f;
            if (in.isDown(mge::Key::S)) camInput.moveForward -= 1.0f;
            if (in.isDown(mge::Key::D)) camInput.moveRight += 1.0f;
            if (in.isDown(mge::Key::A)) camInput.moveRight -= 1.0f;
            if (in.isDown(mge::Key::Space)) camInput.moveUp += 1.0f;
            if (in.isDown(mge::Key::LeftCtrl)) camInput.moveUp -= 1.0f;
            camInput.sprint = in.isDown(mge::Key::LeftShift);

            if (in.isDown(mge::MouseButton::Right) || m_platform->isCursorLocked()) {
                camInput.lookX = in.mouseDelta().x;
                camInput.lookY = in.mouseDelta().y;
            }

            m_fly.update(dt, camInput);
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame || !m_renderer || !m_sdfRenderer) return;

            const float aspect =
                static_cast<float>(frame->width) / static_cast<float>(frame->height > 0 ? frame->height : 1);
            const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, 0.1f, 200.0f);
            const glm::mat4 view = glm::lookAt(m_fly.position(), m_fly.position() + m_fly.forward(),
                                               glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 viewProj = projection * view;
            const glm::mat4 invViewProj = glm::inverse(viewProj);

            if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                  {0.5f, 0.7f, 1.0f, 1.0f})) {
                m_renderer->setCamera(viewProj, m_fly.position());
                m_renderer->setLighting(m_lighting);

                // 1. Geometría opaca rasterizada
                m_renderer->draw(m_groundMesh, glm::mat4(1.0f));
                m_renderer->draw(m_cubeMesh, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

                // 2. Pase de SDF raymarching (DESPUÉS de lo opaco y antes de lo transparente)
                mge::SdfFrame sdfFrame;
                sdfFrame.viewProjection = viewProj;
                sdfFrame.inverseViewProjection = invViewProj;
                sdfFrame.cameraPosition = m_fly.position();
                sdfFrame.time = m_time;
                m_sdfRenderer->draw(*m_renderer, sdfFrame, m_lighting);

                m_renderer->end();
            }
            m_presented++;
        }

        void onShutdown() override {
            m_groundMesh = {};
            m_cubeMesh = {};
            m_sdfRenderer.reset();
            m_renderer.reset();

            MGE_LOG_INFO("SdfScene", m_presented, " frames presentados");
            if (m_presented == 0) setExitCode(1);
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "SDF Raymarching Scene";
            config.width = 960;
            config.height = 540;
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        std::unique_ptr<mge::MeshRenderer> m_renderer;
        std::unique_ptr<mge::SdfRenderer> m_sdfRenderer;
        mge::GpuMesh m_groundMesh;
        mge::GpuMesh m_cubeMesh;
        mge::FlyController m_fly;
        mge::Lighting m_lighting;
        float m_time = 0.0f;
        std::uint64_t m_presented = 0;
    };

} // namespace

mge::Application* mge::createApplication() {
    return new SdfSceneApp();
}
