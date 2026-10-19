#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace mge {

    // Ventana de SDL3 (redimensionable, sin API gráfica propia: la toma la GPU de SDL en el GpuModule)
    class Window {
    public:
        Window(const std::string& title, int width, int height);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Tamaño pedido al crearla (el real lo da el swapchain: GpuFrame::width/height)
        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        // false si falló la creación de la ventana (ver el log)
        bool isValid() const { return m_sdlWindow != nullptr; }

        SDL_Window* getSDLWindow() { return m_sdlWindow; }

    private:
        std::string m_title;
        int m_width;
        int m_height;
        SDL_Window* m_sdlWindow = nullptr;
    };
}
