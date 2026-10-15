#include <mge/platform/InputManager.h>

namespace mge {

    glm::vec2 InputManager::s_mousePos = {0.0f, 0.0f};
    glm::vec2 InputManager::s_mouseDelta = {0.0f, 0.0f};
    bool InputManager::s_cursorLocked = false;
    SDL_Window* InputManager::s_window = nullptr;

    void InputManager::init(SDL_Window* window) {
        s_window = window;
        s_mousePos = {0.0f, 0.0f};
        s_mouseDelta = {0.0f, 0.0f};
        s_cursorLocked = false;
    }

    void InputManager::update() {
        // Reseteamos el delta al inicio de cada frame
        s_mouseDelta = {0.0f, 0.0f};

        // Asegurar que el cursor esté en el estado correcto (Fuerza bruta)
        if (s_cursorLocked) {
            if (s_window && !SDL_GetWindowRelativeMouseMode(s_window)) {
                SDL_SetWindowRelativeMouseMode(s_window, true);
            }
            if (s_window && !SDL_GetWindowMouseGrab(s_window)) {
                SDL_SetWindowMouseGrab(s_window, true);
            }
        }
    }

    void InputManager::processEvent(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            s_mousePos.x = event.motion.x;
            s_mousePos.y = event.motion.y;

            // Acumulamos el movimiento relativo (SDL3 lo da en float)
            s_mouseDelta.x += event.motion.xrel;
            s_mouseDelta.y += event.motion.yrel;
        }
    }

    glm::vec2 InputManager::getMousePosition() {
        return s_mousePos;
    }

    glm::vec2 InputManager::getMouseDelta() {
        return s_mouseDelta;
    }

    void InputManager::setCursorMode(bool locked) {
        s_cursorLocked = locked;
        if (s_window) {
            SDL_SetWindowRelativeMouseMode(s_window, locked);
            SDL_SetWindowMouseGrab(s_window, locked);
        }
    }

    bool InputManager::isCursorLocked() {
        return s_cursorLocked;
    }

    bool InputManager::isKeyPressed(SDL_Scancode key) {
        const bool* state = SDL_GetKeyboardState(nullptr);
        return state[key];
    }
}