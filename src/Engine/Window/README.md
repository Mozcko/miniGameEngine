# src/Engine/Window/

| File | Namespace | Purpose |
|------|-----------|---------|
| `Window.h/.cpp` | `MiniGameEngine` | Creates the SDL2 window, the OpenGL context, initialises GLEW and sets default GL state |

## `MiniGameEngine::Window`

Owned by `Engine::Application` (`std::unique_ptr<Window> m_Window`). Created in the
Application constructor with `("Mini Game Engine - Minecraft Cube", 1280, 720)`.

### What `init()` does (called from the constructor)
1. `SDL_Init(SDL_INIT_VIDEO)` – exits the process on failure.
2. Requests **OpenGL 3.3 Core profile**, double buffering.
3. `SDL_CreateWindow` – centered, flags `OPENGL | SHOWN | RESIZABLE`.
4. `SDL_GL_CreateContext`.
5. `glewExperimental = GL_TRUE; glewInit()`.
6. Default GL state:
   - `glEnable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
   - `glDisable(GL_DEPTH_TEST)` – a 2D‑friendly default (a comment explains sprites were being
     discarded by the depth test). **3D games must enable it** (SandboxApp does in `OnInit`).
7. Prints `OpenGL Version: …`.

Any failure in steps 1–5 prints an error and calls `exit(1)`.

### Public API
| Member | Notes |
|--------|-------|
| `Window(title, width, height)` | Calls `init()`. |
| `~Window()` | Deletes GL context, destroys window, calls `SDL_Quit()`, prints a goodbye message. |
| `swapBuffers()` | `SDL_GL_SwapWindow` – called by `Application::Run` each frame. |
| `getWidth()/getHeight()` | The **initial** size; not updated on resize. |
| `getSDLWindow()` | Raw `SDL_Window*` (used by `InputManager::Init` for mouse grab). |
| `handleEvents()`, `isClosed()`, `close()` | Legacy self‑contained event loop; **unused** – `Application::Run` polls events itself. Don't call `handleEvents()` alongside Run: it would swallow events. |

### Resizing
The window is resizable, but `Window` does nothing on resize. The game must handle
`SDL_WINDOWEVENT_RESIZED` (SandboxApp calls `glViewport` and `Camera3D::updateProjection`).
