#include <mge/platform/Window.h>
#include <mge/core/Log.h>

namespace mge {

    Window::Window(const std::string& title, int width, int height, bool openGl)
        : m_title(title), m_width(width), m_height(height), m_isClosed(false), m_openGl(openGl),
          m_sdlWindow(nullptr), m_glContext(nullptr) {
        m_isValid = init();
    }

    Window::~Window() {
        if (m_glContext) SDL_GL_DestroyContext(m_glContext);
        if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);
        MGE_LOG_INFO("Window", "Ventana cerrada y recursos liberados");
    }

    bool Window::init() {
        // El subsistema de video lo inicializa el PlatformModule (dueño del ciclo de vida de SDL)
        if (!m_openGl) {
            // Sin OpenGL: la ventana la toma la GPU de SDL (Vulkan/Metal/D3D12) en el GpuModule
            m_sdlWindow = SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_RESIZABLE);
            if (m_sdlWindow == nullptr) {
                MGE_LOG_FATAL("Window", "No se pudo crear la ventana SDL: ", SDL_GetError());
                return false;
            }
            return true;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,
                            24); // El default de SDL es 16 bits: z-fighting a distancia (KI#22)

        m_sdlWindow =
            SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

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
        // SDL3 prefiere Wayland (contexto EGL). El GLEW de las distros está compilado para GLX y
        // devuelve GLEW_ERROR_NO_GLX_DISPLAY, pero solo después de cargar las funciones de OpenGL
        // (glewContextInit va primero), así que el contexto es utilizable.
        if (glewStatus == GLEW_ERROR_NO_GLX_DISPLAY) {
            MGE_LOG_DEBUG("Window", "GLEW sin display GLX (contexto EGL/Wayland): se continúa");
            glewStatus = GLEW_OK;
        }
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
        if (m_glContext) SDL_GL_SwapWindow(m_sdlWindow);
    }

    void Window::handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_isClosed = true;
            }
        }
    }
}