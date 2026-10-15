# src/Engine/Input/

| File | Namespace | Purpose |
|------|-----------|---------|
| `InputManager.h/.cpp` | `Engine` | Static keyboard / mouse state and cursor locking |

## `Engine::InputManager` (all static)

### Lifecycle (driven by `Application::Run`, don't call these yourself)
| Call | When | Effect |
|------|------|--------|
| `Init(SDL_Window*)` | once, before `OnInit` | Stores window, resets state, cursor unlocked |
| `Update()` | start of every frame, before event polling | Resets `MouseDelta` to 0; if locked, re‑asserts relative mouse mode + window grab (needed after focus changes) |
| `ProcessEvent(e)` | for every SDL event, *after* the game's `OnEvent` | On `SDL_MOUSEMOTION`: stores absolute position, **accumulates** `xrel/yrel` into the delta |

### Query API
| Method | Returns |
|--------|---------|
| `GetMousePosition()` | `glm::vec2` last absolute mouse position in window pixels (y down) |
| `GetMouseDelta()` | `glm::vec2` total relative motion this frame (y down) |
| `IsKeyPressed(SDL_Scancode)` | Current held state via `SDL_GetKeyboardState` (level, not edge‑triggered) |
| `SetCursorMode(bool locked)` | Locks/unlocks: `SDL_SetRelativeMouseMode` + `SDL_SetWindowGrab` |
| `IsCursorLocked()` | Current lock flag |

### Notes
- There is **no** "pressed this frame" / "released" API and no mouse‑button query. For
  one‑shot actions handle `SDL_KEYDOWN` / `SDL_MOUSEBUTTONDOWN` in `OnEvent` (SandboxApp does this
  for `Esc`, `P` and click‑to‑capture).
- Keys use **scancodes** (physical layout: `SDL_SCANCODE_W`), events use keycodes (`SDLK_p`).
- `Camera3D` only applies mouse look when `IsCursorLocked()`; `FirstPersonCamera` always does.
