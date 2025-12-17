#pragma once

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>

namespace MiniGameEngine {

    class Window {
    public:
        // Constructor: Recibe título, ancho y alto
        Window(const std::string& title, int width, int height);
        // Destructor: Se encarga de limpiar la memoria automáticamente
        ~Window();

        // Intercambia los buffers (Muestra lo que se dibujó)
        void swapBuffers();
        
        // Getters simples
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        bool isClosed() const { return _isClosed; }
        void close() { _isClosed = true; }
        
        // --- ESTA ES LA LÍNEA QUE FALTABA ---
        // Permite acceder a la ventana cruda de SDL desde fuera (necesario para el viewport)
        SDL_Window* getSDLWindow() { return _sdlWindow; }

        // Maneja eventos de ventana (como cerrar)
        void handleEvents();

    private:
        // Función privada interna para inicializar todo
        void init();

        std::string _title;
        int _width;
        int _height;
        bool _isClosed;

        // Punteros crudos de SDL (recursos del sistema operativo)
        SDL_Window* _sdlWindow;
        SDL_GLContext _glContext;
    };

}