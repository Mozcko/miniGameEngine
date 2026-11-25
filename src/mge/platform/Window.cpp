#include <mge/platform/Window.h>
#include <mge/core/Log.h>

namespace mge {

    Window::Window(const std::string& title, int width, int height)
        : m_title(title), m_width(width), m_height(height) {
        // The video subsystem is initialized by PlatformModule (owner of SDL's lifecycle)
        m_sdlWindow = SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_RESIZABLE);
        if (!m_sdlWindow) MGE_LOG_FATAL("Window", "Failed to create SDL window: ", SDL_GetError());
    }

    Window::~Window() {
        if (m_sdlWindow) {
            SDL_DestroyWindow(m_sdlWindow);
            MGE_LOG_INFO("Window", "Window closed and resources released");
        }
    }
}
