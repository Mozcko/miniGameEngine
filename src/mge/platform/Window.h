#pragma once

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>

namespace mge {

    class Window {
    public:
        // Constructor: Recibe título, ancho y alto
        Window(const std::string& title, int width, int height);
        // Destructor: Se encarga de limpiar la memoria automáticamente
        ~Window();

        // Intercambia los buffers (Muestra lo que se dibujó)
        void swapBuffers();

        // Getters simples
        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        bool isClosed() const { return m_isClosed; }
        void close() { m_isClosed = true; }

        // --- ESTA ES LA LÍNEA QUE FALTABA ---
        // Permite acceder a la ventana cruda de SDL desde fuera (necesario para el viewport)
        SDL_Window* getSDLWindow() { return m_sdlWindow; }

        // Maneja eventos de ventana (como cerrar)
        void handleEvents();

    private:
        // Función privada interna para inicializar todo
        void init();

        std::string m_title;
        int m_width;
        int m_height;
        bool m_isClosed;

        // Punteros crudos de SDL (recursos del sistema operativo)
        SDL_Window* m_sdlWindow;
        SDL_GLContext m_glContext;
    };

}