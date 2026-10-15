# src/mge/platform/  →  target `mge_platform`

Window + OpenGL context (GLEW) and input. Depends on `mge_core`, SDL3, GLEW, OpenGL.

## `PlatformModule`

`#include <mge/platform/PlatformModule.h>` → `app.addModule<mge::PlatformModule>()`.

| Hook | Does |
|------|------|
| `onInit` | `SDL_InitSubSystem(SDL_INIT_VIDEO)`, then creates the `Window` from `ApplicationConfig` (title, size). Fails, and so stops startup, if the window, GL context or GLEW fails. Then `InputManager::init(window)` |
| `onFrameBegin` | `InputManager::update()`, then polls every SDL event: `SDL_EVENT_QUIT` → `app.requestStop()`; the event goes to the game's callback (`setEventCallback`), then to `InputManager::processEvent` |
| `onFrameEnd` | `Window::swapBuffers()` |
| `onShutdown` | Clears the InputManager window, destroys the window (GL context), `SDL_QuitSubSystem(SDL_INIT_VIDEO)`; if no SDL subsystem is left, `SDL_Quit()` |

`getWindow()` is valid between init and shutdown. `setEventCallback(std::function<void(const SDL_Event&)>)`
is temporary until input actions (#15).

## Window

| File | Namespace | Purpose |
|------|-----------|---------|
| `Window.h/.cpp` | `mge` | Creates the SDL3 window, the OpenGL context, initialises GLEW and sets default GL state |

## `mge::Window`

Owned by `mge::Application` (`std::unique_ptr<Window> m_window`). Created in the
Application constructor with `("Mini Game Engine - Minecraft Cube", 1280, 720)`.

### What `init()` does (called from the constructor)
1. (SDL video is initialised by `PlatformModule` with `SDL_InitSubSystem` – `Window` doesn't init or quit SDL.)
2. Requests **OpenGL 3.3 Core profile**, double buffering.
3. `SDL_CreateWindow` – centered, flags `OPENGL | SHOWN | RESIZABLE`.
4. `SDL_GL_CreateContext`.
5. `glewExperimental = GL_TRUE; glewInit()`.
6. Default GL state:
   - `glEnable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
   - `glDisable(GL_DEPTH_TEST)` – a 2D‑friendly default (a comment explains sprites were being
     discarded by the depth test). **3D games must enable it** (SandboxApp does in `onInit`).
7. Prints `OpenGL Version: …`.

Any failure in steps 1–5 prints an error and calls `exit(1)`.

### Public API
| Member | Notes |
|--------|-------|
| `Window(title, width, height)` | Calls `init()`. |
| `~Window()` | Deletes the GL context and destroys the window (logs it). Does **not** quit SDL. |
| `swapBuffers()` | `SDL_GL_SwapWindow` – called by `Application::Run` each frame. |
| `getWidth()/getHeight()` | The **initial** size; not updated on resize. |
| `getSDLWindow()` | Raw `SDL_Window*` (used by `InputManager::init` for mouse grab). |
| `handleEvents()`, `isClosed()`, `close()` | Legacy self‑contained event loop; **unused** – `Application::Run` polls events itself. Don't call `handleEvents()` alongside Run: it would swallow events. |

### Resizing
The window is resizable, but `Window` does nothing on resize. The game must handle
`SDL_WINDOWEVENT_RESIZED` (SandboxApp calls `glViewport` and `Camera3D::updateProjection`).

---

## Input
| File | Namespace | Purpose |
|------|-----------|---------|
| `InputManager.h/.cpp` | `mge` | Static keyboard / mouse state and cursor locking |

## `mge::InputManager` (all static)

### Lifecycle (driven by `Application::Run`, don't call these yourself)
| Call | When | Effect |
|------|------|--------|
| `init(SDL_Window*)` | once, before `onInit` | Stores window, resets state, cursor unlocked |
| `update()` | start of every frame, before event polling | Resets `MouseDelta` to 0; if locked, re‑asserts relative mouse mode + window grab (needed after focus changes) |
| `processEvent(e)` | for every SDL event, *after* the game's event callback | On `SDL_EVENT_MOUSE_MOTION`: stores absolute position, **accumulates** `xrel/yrel` into the delta |

### Query API
| Method | Returns |
|--------|---------|
| `getMousePosition()` | `glm::vec2` last absolute mouse position in window pixels (y down) |
| `getMouseDelta()` | `glm::vec2` total relative motion this frame (y down) |
| `isKeyPressed(SDL_Scancode)` | Current held state via `SDL_GetKeyboardState` (level, not edge‑triggered) |
| `setCursorMode(bool locked)` | Locks/unlocks: `SDL_SetRelativeMouseMode` + `SDL_SetWindowGrab` |
| `isCursorLocked()` | Current lock flag |

### Notes
- There is **no** "pressed this frame" / "released" API and no mouse‑button query. For
  one‑shot actions handle `SDL_EVENT_KEY_DOWN` / `SDL_EVENT_MOUSE_BUTTON_DOWN` in the `PlatformModule` event callback (SandboxApp does this
  for `Esc`, `P` and click‑to‑capture).
- Keys use **scancodes** (physical layout: `SDL_SCANCODE_W`), events use keycodes (`SDLK_p`).
- `Camera3D` only applies mouse look when `isCursorLocked()`; `FirstPersonCamera` always does.
