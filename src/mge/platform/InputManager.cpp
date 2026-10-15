#include <mge/platform/InputManager.h>

namespace Engine {

    glm::vec2 InputManager::s_MousePos = {0.0f, 0.0f};
    glm::vec2 InputManager::s_MouseDelta = {0.0f, 0.0f};
    bool InputManager::s_CursorLocked = false;
    SDL_Window* InputManager::s_Window = nullptr;

    void InputManager::Init(SDL_Window* window) {
        s_Window = window;
        s_MousePos = {0.0f, 0.0f};
        s_MouseDelta = {0.0f, 0.0f};
        s_CursorLocked = false;
    }

    void InputManager::Update() {
        // Reseteamos el delta al inicio de cada frame
        s_MouseDelta = {0.0f, 0.0f};

        // Asegurar que el cursor esté en el estado correcto (Fuerza bruta)
        if (s_CursorLocked) {
            if (!SDL_GetRelativeMouseMode()) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (s_Window && !SDL_GetWindowGrab(s_Window)) {
                SDL_SetWindowGrab(s_Window, SDL_TRUE);
            }
        }
    }

    void InputManager::ProcessEvent(const SDL_Event& event) {
        if (event.type == SDL_MOUSEMOTION) {
            s_MousePos.x = (float)event.motion.x;
            s_MousePos.y = (float)event.motion.y;
            
            // Acumulamos el movimiento relativo
            s_MouseDelta.x += (float)event.motion.xrel;
            s_MouseDelta.y += (float)event.motion.yrel;
        }
    }

    glm::vec2 InputManager::GetMousePosition() {
        return s_MousePos;
    }

    glm::vec2 InputManager::GetMouseDelta() {
        return s_MouseDelta;
    }

    void InputManager::SetCursorMode(bool locked) {
        s_CursorLocked = locked;
        SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
        if (s_Window) {
            SDL_SetWindowGrab(s_Window, locked ? SDL_TRUE : SDL_FALSE);
        }
    }

    bool InputManager::IsCursorLocked() {
        return s_CursorLocked;
    }

    bool InputManager::IsKeyPressed(SDL_Scancode key) {
        const Uint8* state = SDL_GetKeyboardState(NULL);
        return state[key];
    }
}