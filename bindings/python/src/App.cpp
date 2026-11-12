#include "App.h"
#include "Audio.h"

#include <mge/core/Clock.h>

#include <string>

namespace mge::python {

    namespace {

        PyApplication* sActiveApp = nullptr;

        mge::Key parseKeyName(const std::string& name) {
            if (name == "W" || name == "w") return mge::Key::W;
            if (name == "A" || name == "a") return mge::Key::A;
            if (name == "S" || name == "s") return mge::Key::S;
            if (name == "D" || name == "d") return mge::Key::D;
            if (name == "Space" || name == "space") return mge::Key::Space;
            if (name == "Escape" || name == "escape" || name == "Esc") return mge::Key::Escape;
            if (name == "Enter" || name == "Return") return mge::Key::Enter;
            if (name == "Tab") return mge::Key::Tab;
            if (name == "LeftShift" || name == "Shift") return mge::Key::LeftShift;
            if (name == "LeftCtrl" || name == "Ctrl") return mge::Key::LeftCtrl;
            if (name == "Up") return mge::Key::Up;
            if (name == "Down") return mge::Key::Down;
            if (name == "Left") return mge::Key::Left;
            if (name == "Right") return mge::Key::Right;
            if (name.size() == 1) {
                char c = name[0];
                if (c >= 'A' && c <= 'Z')
                    return static_cast<mge::Key>(static_cast<int>(mge::Key::A) + (c - 'A'));
                if (c >= 'a' && c <= 'z')
                    return static_cast<mge::Key>(static_cast<int>(mge::Key::A) + (c - 'a'));
                if (c >= '0' && c <= '9')
                    return static_cast<mge::Key>(static_cast<int>(mge::Key::Num0) + (c - '0'));
            }
            return mge::Key::Unknown;
        }

        bool isMouseButtonName(const std::string& name, mge::MouseButton& outBtn) {
            if (name == "MouseLeft" || name == "mouse_left" || name == "Left") {
                outBtn = mge::MouseButton::Left;
                return true;
            }
            if (name == "MouseRight" || name == "mouse_right" || name == "Right") {
                outBtn = mge::MouseButton::Right;
                return true;
            }
            if (name == "MouseMiddle" || name == "mouse_middle" || name == "Middle") {
                outBtn = mge::MouseButton::Middle;
                return true;
            }
            return false;
        }

    } // namespace

    PyApplication* PyApplication::activeApp() {
        return sActiveApp;
    }

    // --- InputState ---
    void InputState::bindAction(const std::string& action, const std::string& keyName) {
        m_actionBindings[action] = keyName;
    }

    void InputState::bindAxis(const std::string& axis, const std::string& negKey, const std::string& posKey) {
        m_axisBindings[axis] = {negKey, posKey};
    }

    bool InputState::isActionDown(const std::string& action) const {
        auto it = m_actionBindings.find(action);
        if (it == m_actionBindings.end()) return false;
        return isKeyDown(it->second);
    }

    bool InputState::isActionPressed(const std::string& action) const {
        auto it = m_actionBindings.find(action);
        if (it == m_actionBindings.end()) return false;
        return isKeyPressed(it->second);
    }

    bool InputState::isActionReleased(const std::string& action) const {
        auto it = m_actionBindings.find(action);
        if (it == m_actionBindings.end()) return false;
        return isKeyReleased(it->second);
    }

    float InputState::getAxis(const std::string& axis) const {
        auto it = m_axisBindings.find(axis);
        if (it == m_axisBindings.end()) return 0.0f;
        float value = 0.0f;
        if (isKeyDown(it->second.posKey)) value += 1.0f;
        if (isKeyDown(it->second.negKey)) value -= 1.0f;
        return value;
    }

    bool InputState::isKeyDown(const std::string& key) const {
        auto injIt = m_injectedKeys.find(key);
        if (injIt != m_injectedKeys.end() && injIt->second) return true;

        if (m_app && m_app->platform()) {
            mge::MouseButton btn;
            if (isMouseButtonName(key, btn)) {
                return m_app->platform()->input().isDown(btn);
            }
            mge::Key k = parseKeyName(key);
            if (k != mge::Key::Unknown) {
                return m_app->platform()->input().isDown(k);
            }
        }
        return false;
    }

    bool InputState::isKeyPressed(const std::string& key) const {
        auto injIt = m_injectedPressed.find(key);
        if (injIt != m_injectedPressed.end() && injIt->second) return true;

        if (m_app && m_app->platform()) {
            mge::MouseButton btn;
            if (isMouseButtonName(key, btn)) {
                return m_app->platform()->input().wasPressed(btn);
            }
            mge::Key k = parseKeyName(key);
            if (k != mge::Key::Unknown) {
                return m_app->platform()->input().wasPressed(k);
            }
        }
        return false;
    }

    bool InputState::isKeyReleased(const std::string& key) const {
        auto relIt = m_injectedReleased.find(key);
        if (relIt != m_injectedReleased.end() && relIt->second) return true;

        if (m_app && m_app->platform()) {
            mge::MouseButton btn;
            if (isMouseButtonName(key, btn)) {
                return m_app->platform()->input().wasReleased(btn);
            }
            mge::Key k = parseKeyName(key);
            if (k != mge::Key::Unknown) {
                return m_app->platform()->input().wasReleased(k);
            }
        }
        return false;
    }

    glm::vec2 InputState::mousePos() const {
        if (m_app && m_app->platform()) {
            const auto p = m_app->platform()->input().mousePosition();
            return glm::vec2(p.x, p.y);
        }
        return glm::vec2(0.0f);
    }

    glm::vec2 InputState::mouseDelta() const {
        return m_mouseDelta;
    }

    bool InputState::isMouseDown(int button) const {
        if (m_app && m_app->platform()) {
            return m_app->platform()->input().isDown(static_cast<mge::MouseButton>(button));
        }
        return false;
    }

    bool InputState::isMousePressed(int button) const {
        if (m_app && m_app->platform()) {
            return m_app->platform()->input().wasPressed(static_cast<mge::MouseButton>(button));
        }
        return false;
    }

    bool InputState::isMouseReleased(int button) const {
        if (m_app && m_app->platform()) {
            return m_app->platform()->input().wasReleased(static_cast<mge::MouseButton>(button));
        }
        return false;
    }

    void InputState::setRelativeMouse(bool locked) {
        if (m_app && m_app->platform()) {
            m_app->platform()->setCursorLocked(locked);
        }
    }

    bool InputState::isRelativeMouse() const {
        if (m_app && m_app->platform()) {
            return m_app->platform()->isCursorLocked();
        }
        return false;
    }

    void InputState::injectKey(const std::string& key, bool down) {
        m_injectedKeys[key] = down;
        if (down) {
            m_injectedPressed[key] = true;
            m_injectedReleased[key] = false;
        } else {
            m_injectedPressed[key] = false;
            m_injectedReleased[key] = true;
        }
    }

    void InputState::updateMouse() {
        if (m_app && m_app->platform()) {
            const auto p = m_app->platform()->input().mousePosition();
            const glm::vec2 curPos{p.x, p.y};
            if (m_firstMouse) {
                m_lastMousePos = curPos;
                m_firstMouse = false;
            }
            m_mouseDelta = curPos - m_lastMousePos;
            m_lastMousePos = curPos;
        }
    }

    // --- PyApplication ---
    PyApplication::PyApplication(const std::string& title, int width, int height, bool headless, int modules,
                                 int tickRate, int maxFrames, bool vsync)
        : PyApplication(
              [&]() {
                  mge::ApplicationConfig cfg;
                  cfg.title = title;
                  cfg.width = width;
                  cfg.height = height;
                  cfg.headless = headless;
                  cfg.tickRate = static_cast<double>(tickRate);
                  cfg.maxFrames = static_cast<std::uint64_t>(maxFrames);
                  cfg.vsync = vsync;
                  return cfg;
              }(),
              static_cast<uint32_t>(modules)) {}

    PyApplication::PyApplication(const mge::ApplicationConfig& config, uint32_t moduleFlags)
        : Application(config), m_moduleFlags(moduleFlags) {
        sActiveApp = this;
        m_pyWorld = std::make_unique<PyWorld>(&Application::world());
        m_pyAudio = std::make_unique<PyAudio>(this);

        if (!config.headless) {
#if defined(MGE_HAS_PLATFORM)
            if (m_moduleFlags & 1) { // Platform
                addModule<mge::PlatformModule>();
#if defined(MGE_HAS_GPU)
                if (m_moduleFlags & 2) { // GPU
                    addModule<mge::GpuModule>();
                }
#endif
#if defined(MGE_HAS_AUDIO)
                if (m_moduleFlags & 8) { // Audio
                    addModule<mge::AudioModule>();
                }
#endif
            }
#endif
        }
    }

    PyApplication::~PyApplication() {
        if (sActiveApp == this) {
            sActiveApp = nullptr;
        }
    }

    bool PyApplication::onInit() {
        m_platform = getModule<mge::PlatformModule>();
        m_gpu = getModule<mge::GpuModule>();
        m_audioModule = getModule<mge::AudioModule>();

        if (m_gpu) {
            m_meshRenderer = std::make_unique<mge::MeshRenderer>(m_gpu->device(), m_gpu->swapchainFormat());
            m_skyRenderer = std::make_unique<mge::SkyRenderer>(m_gpu->device(), m_gpu->swapchainFormat(),
                                                               m_gpu->device().depthFormat());
            m_uiRenderer = std::make_unique<mge::Renderer2D>(m_gpu->device(), m_gpu->swapchainFormat());
        }

        namespace nb = nanobind;
        nb::gil_scoped_acquire gil;
        nb::handle self = nb::detail::trampoline(this).base();
        if (self.is_valid()) {
            if (nb::hasattr(self, "on_init")) {
                nb::object ret = self.attr("on_init")();
                if (ret.is_none()) return true;
                return nb::cast<bool>(ret);
            }
            if (nb::hasattr(self, "onInit")) {
                nb::object ret = self.attr("onInit")();
                if (ret.is_none()) return true;
                return nb::cast<bool>(ret);
            }
        }
        return Application::onInit();
    }

    void PyApplication::onFixedUpdate(float fixedDt) {
        namespace nb = nanobind;
        nb::gil_scoped_acquire gil;
        nb::handle self = nb::detail::trampoline(this).base();
        if (self.is_valid()) {
            if (nb::hasattr(self, "on_fixed_update")) {
                self.attr("on_fixed_update")(fixedDt);
                return;
            }
            if (nb::hasattr(self, "onFixedUpdate")) {
                self.attr("onFixedUpdate")(fixedDt);
                return;
            }
        }
        Application::onFixedUpdate(fixedDt);
    }

    void PyApplication::onUpdate(float dt) {
        m_input.updateMouse();

        namespace nb = nanobind;
        nb::gil_scoped_acquire gil;
        nb::handle self = nb::detail::trampoline(this).base();
        if (self.is_valid()) {
            if (nb::hasattr(self, "on_update")) {
                self.attr("on_update")(dt);
                return;
            }
            if (nb::hasattr(self, "onUpdate")) {
                self.attr("onUpdate")(dt);
                return;
            }
        }
        Application::onUpdate(dt);
    }

    void PyApplication::onRender() {
        m_inRenderPass = false;

        if (m_gpu) {
            const auto* frame = m_gpu->frame();
            if (frame && m_uiRenderer) {
                m_uiRenderer->begin(frame->width, frame->height, frame->displayScale);
            }
        }

        {
            namespace nb = nanobind;
            nb::gil_scoped_acquire gil;
            nb::handle self = nb::detail::trampoline(this).base();
            if (self.is_valid()) {
                if (nb::hasattr(self, "on_render")) {
                    self.attr("on_render")();
                } else if (nb::hasattr(self, "onRender")) {
                    self.attr("onRender")();
                } else {
                    Application::onRender();
                }
            } else {
                Application::onRender();
            }
        }

        if (m_inRenderPass && m_meshRenderer) {
            m_meshRenderer->end();
            m_inRenderPass = false;
        }

        if (m_gpu) {
            const auto* frame = m_gpu->frame();
            if (frame && m_uiRenderer && m_uiRenderer->vertexCount() > 0) {
                m_uiRenderer->flush(*frame->cmd, frame->swapchain);
            }
        }
    }

    void PyApplication::setManualClock(double autoStep) {
        setClock(std::make_unique<mge::ManualClock>(autoStep));
    }

    void PyApplication::onShutdown() {
        {
            namespace nb = nanobind;
            nb::gil_scoped_acquire gil;
            nb::handle self = nb::detail::trampoline(this).base();
            if (self.is_valid()) {
                if (nb::hasattr(self, "on_shutdown")) {
                    self.attr("on_shutdown")();
                } else if (nb::hasattr(self, "onShutdown")) {
                    self.attr("onShutdown")();
                } else {
                    Application::onShutdown();
                }
            } else {
                Application::onShutdown();
            }
            m_pyWorld->clear();
        }

        if (m_gpu) {
            m_gpu->device().tracker().releaseAll();
        }
        m_meshRenderer.reset();
        m_skyRenderer.reset();
        m_uiRenderer.reset();
    }

    void initApp(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- ModuleFlags ---
        enum class PyModuleFlags : uint32_t {
            NONE = 0,
            PLATFORM = 1,
            GPU = 2,
            GRAPHICS = 4,
            AUDIO = 8,
            DEFAULT = 15,
        };
        nb::enum_<PyModuleFlags>(m, "ModuleFlags")
            .value("NONE", PyModuleFlags::NONE)
            .value("PLATFORM", PyModuleFlags::PLATFORM)
            .value("GPU", PyModuleFlags::GPU)
            .value("GRAPHICS", PyModuleFlags::GRAPHICS)
            .value("AUDIO", PyModuleFlags::AUDIO)
            .value("DEFAULT", PyModuleFlags::DEFAULT)
            .export_values();

        // --- Key ---
        nb::enum_<mge::Key>(m, "Key")
            .value("UNKNOWN", mge::Key::Unknown)
            .value("W", mge::Key::W)
            .value("A", mge::Key::A)
            .value("S", mge::Key::S)
            .value("D", mge::Key::D)
            .value("E", mge::Key::E)
            .value("Q", mge::Key::Q)
            .value("R", mge::Key::R)
            .value("F", mge::Key::F)
            .value("SPACE", mge::Key::Space)
            .value("ESCAPE", mge::Key::Escape)
            .value("ENTER", mge::Key::Enter)
            .value("TAB", mge::Key::Tab)
            .value("LEFT_SHIFT", mge::Key::LeftShift)
            .value("LEFT_CTRL", mge::Key::LeftCtrl)
            .value("UP", mge::Key::Up)
            .value("DOWN", mge::Key::Down)
            .value("LEFT", mge::Key::Left)
            .value("RIGHT", mge::Key::Right)
            .export_values();

        // --- MouseButton ---
        nb::enum_<mge::MouseButton>(m, "MouseButton")
            .value("LEFT", mge::MouseButton::Left)
            .value("RIGHT", mge::MouseButton::Right)
            .value("MIDDLE", mge::MouseButton::Middle)
            .export_values();

        // --- Time ---
        nb::class_<mge::Time>(m, "Time")
            .def_prop_ro("delta", &mge::Time::delta)
            .def_prop_ro("fixed_delta", &mge::Time::fixedDelta)
            .def_prop_ro("alpha", &mge::Time::alpha)
            .def_prop_ro("elapsed", &mge::Time::elapsed)
            .def_prop_ro("frame_count", &mge::Time::frameCount)
            .def_prop_ro("fps",
                         [](const mge::Time& t) {
                             return t.unscaledDelta() > 0.00001f ? (1.0f / t.unscaledDelta()) : 0.0f;
                         })
            .def_prop_rw("time_scale", &mge::Time::timeScale, &mge::Time::setTimeScale);

        // --- Input ---
        nb::class_<InputState>(m, "Input")
            .def("bind_action", &InputState::bindAction, nb::arg("action"), nb::arg("key_name"))
            .def("bind_axis", &InputState::bindAxis, nb::arg("axis"), nb::arg("neg_key"), nb::arg("pos_key"))
            .def("is_action_down", &InputState::isActionDown, nb::arg("action"))
            .def("action_down", &InputState::isActionDown, nb::arg("action"))
            .def("is_action_pressed", &InputState::isActionPressed, nb::arg("action"))
            .def("action_pressed", &InputState::isActionPressed, nb::arg("action"))
            .def("is_action_released", &InputState::isActionReleased, nb::arg("action"))
            .def("action_released", &InputState::isActionReleased, nb::arg("action"))
            .def("get_axis", &InputState::getAxis, nb::arg("axis"))
            .def(
                "key_down",
                [](const InputState& in, nb::object key) {
                    if (nb::isinstance<mge::Key>(key)) {
                        mge::Key k = nb::cast<mge::Key>(key);
                        if (k == mge::Key::W) return in.isKeyDown("W");
                        if (k == mge::Key::A) return in.isKeyDown("A");
                        if (k == mge::Key::S) return in.isKeyDown("S");
                        if (k == mge::Key::D) return in.isKeyDown("D");
                        if (k == mge::Key::Space) return in.isKeyDown("Space");
                        if (k == mge::Key::Escape) return in.isKeyDown("Escape");
                    }
                    return in.isKeyDown(nb::cast<std::string>(nb::str(key)));
                },
                nb::arg("key"))
            .def(
                "key_pressed",
                [](const InputState& in, nb::object key) {
                    if (nb::isinstance<mge::Key>(key)) {
                        mge::Key k = nb::cast<mge::Key>(key);
                        if (k == mge::Key::W) return in.isKeyPressed("W");
                        if (k == mge::Key::A) return in.isKeyPressed("A");
                        if (k == mge::Key::S) return in.isKeyPressed("S");
                        if (k == mge::Key::D) return in.isKeyPressed("D");
                        if (k == mge::Key::Space) return in.isKeyPressed("Space");
                        if (k == mge::Key::Escape) return in.isKeyPressed("Escape");
                    }
                    return in.isKeyPressed(nb::cast<std::string>(nb::str(key)));
                },
                nb::arg("key"))
            .def(
                "key_released",
                [](const InputState& in, nb::object key) {
                    return in.isKeyReleased(nb::cast<std::string>(nb::str(key)));
                },
                nb::arg("key"))
            .def_prop_ro("mouse_pos", &InputState::mousePos)
            .def_prop_ro("mouse_delta", &InputState::mouseDelta)
            .def("mouse_down", &InputState::isMouseDown, nb::arg("button") = 0)
            .def("mouse_pressed", &InputState::isMousePressed, nb::arg("button") = 0)
            .def("mouse_released", &InputState::isMouseReleased, nb::arg("button") = 0)
            .def("set_relative_mouse", &InputState::setRelativeMouse, nb::arg("locked"))
            .def("is_relative_mouse", &InputState::isRelativeMouse)
            .def("inject_key", &InputState::injectKey, nb::arg("key"), nb::arg("down"));

        // --- Application ---
        nb::class_<mge::Application, PyApplication>(m, "Application")
            .def(nb::init<const std::string&, int, int, bool, int, int, int, bool>(),
                 nb::arg("title") = "miniGameEngine", nb::arg("width") = 1280, nb::arg("height") = 720,
                 nb::arg("headless") = false, nb::arg("modules") = 15, nb::arg("tick_rate") = 60,
                 nb::arg("max_frames") = 0, nb::arg("vsync") = true)
            .def("run", &PyApplication::run, nb::call_guard<nb::gil_scoped_release>())
            .def("request_stop", &PyApplication::requestStop)
            .def(
                "set_manual_clock",
                [](mge::Application& baseApp, double autoStep) {
                    static_cast<PyApplication&>(baseApp).setManualClock(autoStep);
                },
                nb::arg("auto_step") = 1.0 / 60.0)
            .def_prop_ro("is_running", &mge::Application::isRunning)
            .def_prop_ro(
                "time", [](mge::Application& app) -> mge::Time& { return app.time(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "input",
                [](mge::Application& baseApp) -> InputState& {
                    return static_cast<PyApplication&>(baseApp).input();
                },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "world",
                [](mge::Application& baseApp) -> PyWorld& {
                    return static_cast<PyApplication&>(baseApp).world();
                },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "audio",
                [](mge::Application& baseApp) -> PyAudio& {
                    return static_cast<PyApplication&>(baseApp).audio();
                },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "renderer",
                [](mge::Application& baseApp) -> PyRenderer& {
                    return static_cast<PyApplication&>(baseApp).renderer();
                },
                nb::rv_policy::reference_internal)
            .def_prop_ro("viewport", [](mge::Application& app) {
                const auto vp = app.viewport();
                return nb::make_tuple(vp.x, vp.y);
            });
    }

} // namespace mge::python
