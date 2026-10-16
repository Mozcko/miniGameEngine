#include <mge/platform/Window.h>
#include <mge/core/Log.h>

namespace mge {

    Window::Window(const std::string& title, int width, int height)
        : m_title(title), m_width(width), m_height(height) {
        // El subsistema de video lo inicializa el PlatformModule (dueño del ciclo de vida de SDL)
        m_sdlWindow = SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_RESIZABLE);
        if (!m_sdlWindow) MGE_LOG_FATAL("Window", "No se pudo crear la ventana SDL: ", SDL_GetError());
    }

    Window::~Window() {
        if (m_sdlWindow) {
            SDL_DestroyWindow(m_sdlWindow);
            MGE_LOG_INFO("Window", "Ventana cerrada y recursos liberados");
        }
    }
}
