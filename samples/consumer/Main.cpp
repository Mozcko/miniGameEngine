// Juego de demostración del paquete reutilizable miniGameEngine.
// Limpia la pantalla, dibuja un cubo procedural que gira y sale con Escape.
#include <mge/mge.h>

#include <glm/gtc/matrix_transform.hpp>
#include <memory>

namespace consumer {

    class ConsumerApp : public mge::Application {
    public:
        ConsumerApp() : mge::Application(makeConfig()) {
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
            if (!m_renderer) return false;

            // Cubo procedural unitario de color cian
            m_cubeMesh =
                device.uploadMesh(mge::primitives::box(glm::vec3(1.0f), glm::vec4(0.2f, 0.7f, 0.9f, 1.0f)));

            MGE_LOG_INFO("Consumer", "Consumidor iniciado correctamente. Esc para salir.");
            return true;
        }

        void onShutdown() override { m_renderer.reset(); }

        void onUpdate(float dt) override {
            if (m_platform->wasActionPressed("Quit")) {
                requestStop();
                return;
            }
            m_angle += dt * 1.5f;
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame || !m_renderer) return;

            const float aspect =
                static_cast<float>(frame->width) / static_cast<float>(frame->height > 0 ? frame->height : 1);
            const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, 0.1f, 100.0f);
            const glm::vec3 cameraPos(0.0f, 1.5f, 3.0f);
            const glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 model(1.0f);
            model = glm::rotate(model, m_angle, glm::vec3(0.5f, 1.0f, 0.0f));

            if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                  glm::vec4(0.12f, 0.15f, 0.20f, 1.0f))) {
                m_renderer->setCamera(proj * view, cameraPos);
                m_renderer->draw(m_cubeMesh, model);
                m_renderer->end();
            }
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "miniGameEngine Consumer";
            config.width = 800;
            config.height = 600;
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        std::unique_ptr<mge::MeshRenderer> m_renderer;
        mge::GpuMesh m_cubeMesh;
        float m_angle = 0.0f;
    };

} // namespace consumer

mge::Application* mge::createApplication() {
    return new consumer::ConsumerApp();
}
