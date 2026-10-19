# src/mge/platform/  →  target `mge_platform`

The SDL3 window and the SDL3 → core input translation. Depends on `mge_core` and SDL3. Drawing is
done by the GPU modules ([`mge_gpu`](../gpu/README.md), [`mge_graphics`](../graphics/README.md)).
The OpenGL context and GLEW were removed in #40.

## `PlatformModule`

`#include <mge/platform/PlatformModule.h>` → `addModule<mge::PlatformModule>()`. To draw, add a
`GpuModule` **after** it, because the GPU claims the window.

| Hook | Does |
|------|------|
| `onInit` | `SDL_InitSubSystem(SDL_INIT_VIDEO)`, then creates the `Window` from `ApplicationConfig` (title, size). Startup stops if the window can't be created |
| `onFrameBegin` | `input().beginFrame()`; re‑applies the cursor lock if the OS dropped it. Then it polls every SDL event: `SDL_EVENT_QUIT` → `app.requestStop()`, the raw event goes to the game's callback (window events), then `applySdlEvent(input, event)` |
| `onShutdown` | Clears the input, destroys the window, then `SDL_QuitSubSystem(SDL_INIT_VIDEO)`. If no SDL subsystem is left, `SDL_Quit()` |

| API | Use |
|-----|-----|
| `input()` | The frame's `mge::InputState` (core): keys, buttons, mouse position/delta, wheel, with edges |
| `actions()` | The game's `mge::ActionMap` (bind in `onInit`) |
| `isActionDown(a)`, `wasActionPressed(a)`, `axis(a)` | Shortcuts: an action or axis evaluated on the current input |
| `setCursorLocked(bool)`, `isCursorLocked()` | Relative mouse mode + grab (FPS cameras); re‑applied every frame while requested |
| `setEventCallback(fn)` | Raw SDL events for **non‑input** needs (focus, …). Called before the input update |
| `getWindow()` | Valid between init and shutdown |

## `mge::Window` (`Window.h/.cpp`)

A resizable SDL3 window with no graphics API of its own; `GpuModule` claims it for SDL GPU. It is
non‑copyable and owned by `PlatformModule`.

| Member | Notes |
|--------|-------|
| `Window(title, width, height)` | `SDL_CreateWindow(…, SDL_WINDOW_RESIZABLE)`. SDL video is initialised by `PlatformModule`, not here. On failure it logs, and `isValid()` returns false |
| `~Window()` | Destroys the window (logged). Does **not** quit SDL |
| `getWidth()/getHeight()` | The **requested** size. The real size is the swapchain's (`GpuFrame::width/height`, `Application::viewport()`) |
| `getSDLWindow()` | The raw `SDL_Window*`, used by `GpuModule` and for mouse grab |

### Resizing
Nothing to do:
- the SDL GPU swapchain follows the window;
- `GpuModule` writes its size to `Application::viewport()`;
- `CameraSystem` adjusts every `autoAspect` camera before the Render phase.

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
