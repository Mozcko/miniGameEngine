#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace mge {

    class InputManager {
    public:
        static void init(SDL_Window* window);
        static void update();
        static void processEvent(const SDL_Event& event);

        // Mouse
        static glm::vec2 getMousePosition();
        static glm::vec2 getMouseDelta();
        static void setCursorMode(bool locked);
        static bool isCursorLocked();

        // Keyboard
        static bool isKeyPressed(SDL_Scancode key);

    private:
        static glm::vec2 s_mousePos;
        static glm::vec2 s_mouseDelta;
        static bool s_cursorLocked;
        static SDL_Window* s_window;
    };
}