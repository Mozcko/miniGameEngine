#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace mge {

    // SDL3 window (resizable, no graphics API of its own: claimed by SDL GPU in GpuModule)
    class Window {
    public:
        Window(const std::string& title, int width, int height);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Requested size at creation (actual size comes from the swapchain: GpuFrame::width/height)
        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        // false if window creation failed (check the log)
        bool isValid() const { return m_sdlWindow != nullptr; }

        SDL_Window* getSDLWindow() { return m_sdlWindow; }

    private:
        std::string m_title;
        int m_width;
        int m_height;
        SDL_Window* m_sdlWindow = nullptr;
    };
}
