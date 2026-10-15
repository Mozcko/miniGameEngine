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
            if (!SDL_GetRelativeMouseMode()) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (s_window && !SDL_GetWindowGrab(s_window)) {
                SDL_SetWindowGrab(s_window, SDL_TRUE);
            }
        }
    }

    void InputManager::processEvent(const SDL_Event& event) {
        if (event.type == SDL_MOUSEMOTION) {
            s_mousePos.x = (float)event.motion.x;
            s_mousePos.y = (float)event.motion.y;
            
            // Acumulamos el movimiento relativo
            s_mouseDelta.x += (float)event.motion.xrel;
            s_mouseDelta.y += (float)event.motion.yrel;
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
        SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
        if (s_window) {
            SDL_SetWindowGrab(s_window, locked ? SDL_TRUE : SDL_FALSE);
        }
    }

    bool InputManager::isCursorLocked() {
        return s_cursorLocked;
    }

    bool InputManager::isKeyPressed(SDL_Scancode key) {
        const Uint8* state = SDL_GetKeyboardState(NULL);
        return state[key];
    }
}