#pragma once
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

namespace Engine {

    class InputManager {
    public:
        static void Init(SDL_Window* window);
        static void Update();
        static void ProcessEvent(const SDL_Event& event);

        // Mouse
        static glm::vec2 GetMousePosition();
        static glm::vec2 GetMouseDelta();
        static void SetCursorMode(bool locked);
        static bool IsCursorLocked();

        // Keyboard
        static bool IsKeyPressed(SDL_Scancode key);

    private:
        static glm::vec2 s_MousePos;
        static glm::vec2 s_MouseDelta;
        static bool s_CursorLocked;
        static SDL_Window* s_Window;
    };
}