#include <mge/platform/PlatformModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/platform/SdlInput.h>

#include <algorithm>

namespace mge {

    namespace {
        // Each module releases only its own subsystem; the last one out shuts down SDL completely.
        // (SDL tracks reference counts per subsystem.)
        void quitSdlSubsystem(SDL_InitFlags flags) {
            SDL_QuitSubSystem(flags);
            if (SDL_WasInit(0) == 0) SDL_Quit();
        }
    }

    bool PlatformModule::onInit(Application& app) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            MGE_LOG_FATAL("Platform", "Failed to initialize SDL video: ", SDL_GetError());
            return false;
        }

        const ApplicationConfig& config = app.getConfig();
        m_window = std::make_unique<Window>(config.title, config.width, config.height);
        if (!m_window->isValid()) {
            m_window.reset(); // Reason is already logged
            quitSdlSubsystem(SDL_INIT_VIDEO);
            return false;
        }

        if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            m_gamepadSubsystemInit = true;
            int count = 0;
            SDL_JoystickID* ids = SDL_GetGamepads(&count);
            if (ids != nullptr) {
                for (int i = 0; i < count; ++i) {
                    openGamepad(ids[i]);
                }
                SDL_free(ids);
            }
        } else {
            MGE_LOG_WARN("Platform", "Failed to initialize SDL gamepad subsystem: ", SDL_GetError());
        }

        return true;
    }

    void PlatformModule::onFrameBegin(Application& app) {
        m_input.beginFrame();

        // The OS may release cursor capture (e.g., alt-tab); re-apply while requested
        if (m_cursorLocked) {
            SDL_Window* window = m_window->getSDLWindow();
            if (!SDL_GetWindowRelativeMouseMode(window)) SDL_SetWindowRelativeMouseMode(window, true);
            if (!SDL_GetWindowMouseGrab(window)) SDL_SetWindowMouseGrab(window, true);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) app.requestStop();
            if (m_eventCallback) m_eventCallback(event);

            if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                openGamepad(event.gdevice.which);
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                closeGamepad(event.gdevice.which);
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                       event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                if (auto slot = findGamepadSlot(event.gbutton.which)) {
                    applySdlEvent(m_input, event, *slot);
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                if (auto slot = findGamepadSlot(event.gaxis.which)) {
                    applySdlEvent(m_input, event, *slot);
                }
            } else {
                applySdlEvent(m_input, event);
            }
        }
    }

    bool PlatformModule::rumbleGamepad(std::size_t gamepad, float lowFrequency, float highFrequency,
                                       std::uint32_t durationMs) {
        if (gamepad >= InputState::kMaxGamepads || m_gamepads[gamepad].handle == nullptr) {
            return false;
        }
        const auto low = static_cast<Uint16>(std::clamp(lowFrequency, 0.0f, 1.0f) * 65535.0f);
        const auto high = static_cast<Uint16>(std::clamp(highFrequency, 0.0f, 1.0f) * 65535.0f);
        return SDL_RumbleGamepad(m_gamepads[gamepad].handle, low, high, durationMs);
    }

    void PlatformModule::startTextInput() {
        m_textInputActive = true;
        if (!m_window) return;
        if (!SDL_StartTextInput(m_window->getSDLWindow())) {
            MGE_LOG_WARN("Platform", "Failed to start text input: ", SDL_GetError());
        }
    }

    void PlatformModule::stopTextInput() {
        m_textInputActive = false;
        m_input.clearTextComposition();
        if (!m_window) return;
        if (!SDL_StopTextInput(m_window->getSDLWindow())) {
            MGE_LOG_WARN("Platform", "Failed to stop text input: ", SDL_GetError());
        }
    }

    void PlatformModule::setCursorLocked(bool locked) {
        m_cursorLocked = locked;
        if (!m_window) return;
        SDL_SetWindowRelativeMouseMode(m_window->getSDLWindow(), locked);
        SDL_SetWindowMouseGrab(m_window->getSDLWindow(), locked);
    }

    void PlatformModule::onShutdown(Application& /*app*/) {
        if (m_textInputActive) stopTextInput();

        for (std::size_t i = 0; i < m_gamepads.size(); ++i) {
            if (m_gamepads[i].handle != nullptr) {
                SDL_CloseGamepad(m_gamepads[i].handle);
                m_gamepads[i] = {};
                m_input.setGamepadConnected(i, false);
            }
        }

        m_input.clear();
        // Window first, then the subsystems
        m_window.reset();
        if (m_gamepadSubsystemInit) {
            quitSdlSubsystem(SDL_INIT_GAMEPAD);
            m_gamepadSubsystemInit = false;
        }
        quitSdlSubsystem(SDL_INIT_VIDEO);
    }

    void PlatformModule::openGamepad(SDL_JoystickID id) {
        if (findGamepadSlot(id).has_value()) return;

        std::size_t freeSlot = InputState::kMaxGamepads;
        for (std::size_t i = 0; i < m_gamepads.size(); ++i) {
            if (m_gamepads[i].handle == nullptr) {
                freeSlot = i;
                break;
            }
        }
        if (freeSlot == InputState::kMaxGamepads) {
            MGE_LOG_WARN("Platform", "Ignoring gamepad ", id, ": all ", InputState::kMaxGamepads,
                         " slots are full");
            return;
        }

        SDL_Gamepad* handle = SDL_OpenGamepad(id);
        if (handle == nullptr) {
            MGE_LOG_WARN("Platform", "Failed to open gamepad ", id, ": ", SDL_GetError());
            return;
        }

        const char* rawName = SDL_GetGamepadName(handle);
        const std::string padName = rawName != nullptr ? rawName : "Gamepad";
        m_gamepads[freeSlot] = {handle, id};
        m_input.setGamepadConnected(freeSlot, true, padName);
        MGE_LOG_INFO("Platform", "Gamepad connected in slot ", freeSlot, ": ", padName);
    }

    void PlatformModule::closeGamepad(SDL_JoystickID id) {
        auto slotIndex = findGamepadSlot(id);
        if (!slotIndex) return;

        SDL_CloseGamepad(m_gamepads[*slotIndex].handle);
        m_gamepads[*slotIndex] = {};
        m_input.setGamepadConnected(*slotIndex, false);
        MGE_LOG_INFO("Platform", "Gamepad disconnected from slot ", *slotIndex);
    }

    std::optional<std::size_t> PlatformModule::findGamepadSlot(SDL_JoystickID id) const {
        for (std::size_t i = 0; i < m_gamepads.size(); ++i) {
            if (m_gamepads[i].handle != nullptr && m_gamepads[i].id == id) {
                return i;
            }
        }
        return std::nullopt;
    }
}
