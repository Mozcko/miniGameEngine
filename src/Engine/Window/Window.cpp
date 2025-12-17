#include "Window.h"
#include <iostream>

namespace MiniGameEngine {

    Window::Window(const std::string& title, int width, int height) 
        : _title(title), _width(width), _height(height), _isClosed(false), _sdlWindow(nullptr), _glContext(nullptr)
    {
        init();
    }

    Window::~Window() {
        if (_glContext) SDL_GL_DeleteContext(_glContext);
        if (_sdlWindow) SDL_DestroyWindow(_sdlWindow);
        SDL_Quit();
        std::cout << "Ventana cerrada y recursos liberados correctamente.\n";
    }

    void Window::init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "Error fatal al inicializar SDL: " << SDL_GetError() << std::endl;
            exit(1);
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        _sdlWindow = SDL_CreateWindow(
            _title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            _width, _height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE 
        );

        if (_sdlWindow == nullptr) {
            std::cerr << "Error al crear ventana SDL: " << SDL_GetError() << std::endl;
            exit(1);
        }

        _glContext = SDL_GL_CreateContext(_sdlWindow);
        if (_glContext == nullptr) {
            std::cerr << "Error al crear contexto OpenGL: " << SDL_GetError() << std::endl;
            exit(1);
        }

        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Error al inicializar GLEW" << std::endl;
            exit(1);
        }
        
        // --- CONFIGURACIÓN CRÍTICA ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // >>>>> ESTA ES LA LÍNEA QUE FALTABA EN TU ARCHIVO <<<<<
        // Sin esto, OpenGL borra tus sprites creyendo que son errores 3D
        glDisable(GL_DEPTH_TEST);
        // >>>>> FIN DE LA CORRECCIÓN <<<<<

        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    }

    void Window::swapBuffers() {
        SDL_GL_SwapWindow(_sdlWindow);
    }
    
    void Window::handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                _isClosed = true;
            }
        }
    }
}