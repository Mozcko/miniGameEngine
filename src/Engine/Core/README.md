# src/Engine/Core/

Application lifecycle and engine‑wide services.

| File | Namespace | Purpose |
|------|-----------|---------|
| `Application.h/.cpp` | `Engine` | Base class for games; initialises SDL/audio/TTF, owns the Window, runs the loop |
| `Time.h/.cpp` | `MiniGameEngine` | Static clock with time scale (pause/slow‑mo) and hitch clamping |
| `ResourceManager.h/.cpp` | `MiniGameEngine` | Static texture cache keyed by file path (currently unused) |

---

## `Engine::Application`

### Interface
```cpp
class Application {
public:
    Application();
    virtual ~Application();
    void Run();                                  // blocks until m_Running == false

    virtual void OnInit() = 0;                   // once, after window + input exist
    virtual void OnUpdate(float dt) = 0;         // every frame, dt in seconds
    virtual void OnRender() = 0;                 // every frame, after OnUpdate
    virtual void OnEvent(SDL_Event& event) {}    // every SDL event, before InputManager sees it

    MiniGameEngine::Window& GetWindow();
protected:
    bool m_Running;                              // set false to exit
    std::unique_ptr<MiniGameEngine::Window> m_Window;
private:
    void CalculateDeltaTime();                   // SDL performance counter based
    float m_DeltaTime; Uint64 m_LastFrameTime;
};
Application* CreateApplication();                // defined by the game
```

### Constructor (in order)
1. `AudioManager::init()`
2. `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)` – on failure sets `m_Running = false` and returns
   (the window is then never created → `Run()` would dereference null).
3. Creates `Window("Mini Game Engine - Minecraft Cube", 1280, 720)` – title and size are hard‑coded here.
4. `TTF_Init()` (error printed with `printf`, not fatal).

### Destructor
`AudioManager::shutdown()` → `TTF_Quit()` → `SDL_Quit()`; then `m_Window` is destroyed.

### `Run()`
```
InputManager::Init(window) → OnInit() → loop:
    CalculateDeltaTime → InputManager::Update → poll events (SDL_QUIT ends loop; OnEvent; InputManager::ProcessEvent)
    → OnUpdate(m_DeltaTime) → OnRender() → m_Window->swapBuffers()
```
No frame cap; vsync depends on driver defaults (no `SDL_GL_SetSwapInterval` call). `m_DeltaTime`
is **not clamped** (unlike `Time`).

---

## `MiniGameEngine::Time`

All‑static clock. Must be driven by the game: call `Time::init()` once and `Time::update()`
once per frame (SandboxApp does this at the start of `OnUpdate`).

| Method | Returns / effect |
|--------|------------------|
| `init()` | Stores the current performance counter. |
| `update()` | Computes raw dt; **clamps to 0.1 s** (protects against window drags / breakpoints); `deltaTime = raw * timeScale`. |
| `getDeltaTime()` | Scaled dt – use for gameplay (0 when paused). |
| `getUnscaledDeltaTime()` | Real dt – use for UI, animations that run while paused. |
| `getTimeScale()` / `setTimeScale(s)` | 1 = normal, 0 = paused, 0.5 = slow motion. |

---

## `MiniGameEngine::ResourceManager`

All‑static texture cache: `std::map<std::string, Texture>`.

| Method | Behaviour |
|--------|-----------|
| `getTexture(path)` | Returns cached `Texture` copy, or loads it with `Texture::load`, caches it, returns it. Logs `[ResourceManager] Cargando nueva textura desde disco: …`. Load failures are cached too (texture id 0). |
| `clear()` | Clears the map. **Does not free GPU memory** – `Texture`'s destructor intentionally doesn't call `glDeleteTextures`; you'd need to call `destroy()` on each entry first. |

Returned `Texture`s are cheap value copies sharing one GL id (see Graphics/Core/README.md).
Not used by SandboxApp (it owns its `Texture tilesetTexture` directly).
