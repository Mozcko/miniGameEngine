#include <mge/platform/Window.h>
#include <mge/core/Log.h>

namespace mge {

    Window::Window(const std::string& title, int width, int height)
        : m_title(title), m_width(width), m_height(height), m_isClosed(false), m_sdlWindow(nullptr),
          m_glContext(nullptr) {
        m_isValid = init();
    }

    Window::~Window() {
        if (m_glContext) SDL_GL_DeleteContext(m_glContext);
        if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);
        SDL_Quit();
        MGE_LOG_INFO("Window", "Ventana cerrada y recursos liberados");
    }

    bool Window::init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            MGE_LOG_FATAL("Window", "No se pudo inicializar SDL: ", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        m_sdlWindow =
            SDL_CreateWindow(m_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width,
                             m_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        if (m_sdlWindow == nullptr) {
            MGE_LOG_FATAL("Window", "No se pudo crear la ventana SDL: ", SDL_GetError());
            return false;
        }

        m_glContext = SDL_GL_CreateContext(m_sdlWindow);
        if (m_glContext == nullptr) {
            MGE_LOG_FATAL("Window", "No se pudo crear el contexto OpenGL: ", SDL_GetError());
            return false;
        }

        glewExperimental = GL_TRUE;
        GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK) {
            MGE_LOG_FATAL("Window", "No se pudo inicializar GLEW: ",
                          reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
            return false;
        }

        // --- CONFIGURACIÓN CRÍTICA ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // >>>>> ESTA ES LA LÍNEA QUE FALTABA EN TU ARCHIVO <<<<<
        // Sin esto, OpenGL borra tus sprites creyendo que son errores 3D
        glDisable(GL_DEPTH_TEST);
        // >>>>> FIN DE LA CORRECCIÓN <<<<<

        MGE_LOG_INFO("Window", "OpenGL ", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        return true;
    }

    void Window::swapBuffers() {
        SDL_GL_SwapWindow(m_sdlWindow);
    }

    void Window::handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                m_isClosed = true;
            }
        }
    }
}