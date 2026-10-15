# src/mge/platform/  →  target `mge_platform`

Window + OpenGL context (GLEW) and the SDL3 → core input translation. Depends on `mge_core`, SDL3, GLEW, OpenGL.

## `PlatformModule`

`#include <mge/platform/PlatformModule.h>` → `app.addModule<mge::PlatformModule>()`.

| Hook | Does |
|------|------|
| `onInit` | `SDL_InitSubSystem(SDL_INIT_VIDEO)`, then creates the `Window` from `ApplicationConfig` (title, size). Fails, and so stops startup, if the window, GL context or GLEW fails |
| `onFrameBegin` | `input().beginFrame()`; re‑applies cursor lock if the OS dropped it; polls every SDL event: `SDL_EVENT_QUIT` → `app.requestStop()`, the raw event goes to the game's callback (window events), then `applySdlEvent(input, event)` |
| `onFrameEnd` | `Window::swapBuffers()` |
| `onShutdown` | Clears the input, destroys the window (GL context), `SDL_QuitSubSystem(SDL_INIT_VIDEO)`; if no SDL subsystem is left, `SDL_Quit()` |

| API | Use |
|-----|-----|
| `input()` | The frame's `mge::InputState` (core): keys, buttons, mouse position/delta, wheel, with edges |
| `actions()` | The game's `mge::ActionMap` (bind in `onInit`) |
| `isActionDown(a)`, `wasActionPressed(a)`, `axis(a)` | Shortcuts: action or axis evaluated on the current input |
| `setCursorLocked(bool)`, `isCursorLocked()` | Relative mouse mode + grab (FPS cameras); re‑applied every frame while requested |
| `setEventCallback(fn)` | Raw SDL events for **non‑input** needs (resize, focus). Called before the input update |
| `getWindow()` | Valid between init and shutdown |

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
| `getSDLWindow()` | Raw `SDL_Window*` (used by `PlatformModule` for relative mouse mode / grab). |
| `handleEvents()`, `isClosed()`, `close()` | Legacy self‑contained event loop; **unused** – `Application::Run` polls events itself. Don't call `handleEvents()` alongside Run: it would swallow events. |

### Resizing
The window is resizable, but `Window` does nothing on resize. The game must handle
`SDL_WINDOWEVENT_RESIZED` (SandboxApp calls `glViewport` and `Camera3D::updateProjection`).

---

## Input translation – `SdlInput.h/.cpp`

Pure functions, unit‑tested without a window (`tests/platform/test_sdl_input.cpp`):

| Function | Does |
|----------|------|
| `keyFromScancode(SDL_Scancode)` | Physical key → `mge::Key` (letters, digits, F1–F12, arrows, modifiers, Space/Enter/Esc/Tab/Backspace; others → `Key::Unknown`) |
| `mouseButtonFromSdl(Uint8)` | → `mge::MouseButton` (`Count` if unmapped) |
| `applySdlEvent(InputState&, const SDL_Event&)` | Key down/up (**auto‑repeat ignored**), mouse buttons, motion (absolute position + accumulated relative delta), wheel; **focus lost → `clear()`**, so keys don't stick |

The input state and action maps themselves live in the core (`src/mge/core/Input.h`). Headless
tests can inject input without SDL.

