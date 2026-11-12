#pragma once

#include "Common.h"
#include "Ecs.h"
#include "Render.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace mge::python {

    class PyAudio;

    class InputState {
    public:
        explicit InputState(PyApplication* app) : m_app(app) {}

        void bindAction(const std::string& action, const std::string& keyName);
        void bindAxis(const std::string& axis, const std::string& negKey, const std::string& posKey);

        bool isActionDown(const std::string& action) const;
        bool isActionPressed(const std::string& action) const;
        bool isActionReleased(const std::string& action) const;
        float getAxis(const std::string& axis) const;

        bool isKeyDown(const std::string& key) const;
        bool isKeyPressed(const std::string& key) const;
        bool isKeyReleased(const std::string& key) const;

        glm::vec2 mousePos() const;
        glm::vec2 mouseDelta() const;

        bool isMouseDown(int button) const;
        bool isMousePressed(int button) const;
        bool isMouseReleased(int button) const;

        void setRelativeMouse(bool locked);
        bool isRelativeMouse() const;

        void injectKey(const std::string& key, bool down);

        void updateMouse();

    private:
        PyApplication* m_app = nullptr;
        std::unordered_map<std::string, std::string> m_actionBindings;
        struct AxisBinding {
            std::string negKey;
            std::string posKey;
        };
        std::unordered_map<std::string, AxisBinding> m_axisBindings;
        std::unordered_map<std::string, bool> m_injectedKeys;
        std::unordered_map<std::string, bool> m_injectedPressed;
        std::unordered_map<std::string, bool> m_injectedReleased;
        glm::vec2 m_mouseDelta{0.0f};
        glm::vec2 m_lastMousePos{0.0f};
        bool m_firstMouse = true;
    };

    class PyApplication : public mge::Application {
    public:
        NB_TRAMPOLINE(mge::Application, 5);

        PyApplication(const std::string& title = "miniGameEngine", int width = 1280, int height = 720,
                      bool headless = false, int modules = 15, int tickRate = 60, int maxFrames = 0,
                      bool vsync = true);
        PyApplication(const mge::ApplicationConfig& config, uint32_t moduleFlags);
        ~PyApplication() override;

        bool onInit() override;
        void onFixedUpdate(float fixedDt) override;
        void onUpdate(float dt) override;
        void onRender() override;
        void onShutdown() override;

        void setManualClock(double autoStep = 1.0 / 60.0);

        PyRenderer& renderer() { return m_renderer; }
        PyWorld& world() { return *m_pyWorld; }
        PyAudio& audio() { return *m_pyAudio; }
        InputState& input() { return m_input; }

        mge::PlatformModule* platform() const { return m_platform; }
        mge::GpuModule* gpu() const { return m_gpu; }
        mge::AudioModule* audioModule() const { return m_audioModule; }

        mge::MeshRenderer* meshRenderer() const { return m_meshRenderer.get(); }
        mge::SkyRenderer* skyRenderer() const { return m_skyRenderer.get(); }
        mge::Renderer2D* uiRenderer() const { return m_uiRenderer.get(); }
        mge::DayNight& dayNight() { return m_dayNight; }

        bool inRenderPass() const { return m_inRenderPass; }
        void setInRenderPass(bool inPass) { m_inRenderPass = inPass; }

        const glm::mat4& viewMatrix() const { return m_viewMatrix; }
        const glm::mat4& projMatrix() const { return m_projMatrix; }
        void setViewMatrix(const glm::mat4& v) {
            m_viewMatrix = v;
            m_customCamera = true;
        }
        void setProjMatrix(const glm::mat4& p) {
            m_projMatrix = p;
            m_customCamera = true;
        }
        bool customCamera() const { return m_customCamera; }
        const mge::Lighting& currentLighting() const { return m_currentLighting; }
        void setCurrentLighting(const mge::Lighting& l) { m_currentLighting = l; }

        static PyApplication* activeApp();

    private:
        uint32_t m_moduleFlags = 0;
        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        mge::AudioModule* m_audioModule = nullptr;

        std::unique_ptr<mge::MeshRenderer> m_meshRenderer;
        std::unique_ptr<mge::SkyRenderer> m_skyRenderer;
        std::unique_ptr<mge::Renderer2D> m_uiRenderer;
        mge::DayNight m_dayNight;

        glm::mat4 m_viewMatrix{1.0f};
        glm::mat4 m_projMatrix{1.0f};
        bool m_customCamera = false;
        mge::Lighting m_currentLighting{};
        bool m_inRenderPass = false;

        PyRenderer m_renderer{this};
        InputState m_input{this};
        std::unique_ptr<PyWorld> m_pyWorld;
        std::unique_ptr<PyAudio> m_pyAudio;
    };

} // namespace mge::python
