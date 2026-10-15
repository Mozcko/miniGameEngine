# src/mge/app/  →  targets `mge_app`, `mge_main`

Ties the modules together: creates the window, initialises audio and SDL_ttf, and runs the game
loop. **Temporary home**: in #11 `Application` moves into `mge_core` as a module‑based, headless‑capable
runner, and platform/audio become optional modules.

| File | Target | Purpose |
|------|--------|---------|
| `Application.h/.cpp` | `mge_app` | Base class for games (namespace `Engine`) |
| `Main.cpp` | `mge_main` (OBJECT library) | `main()`: `CreateApplication()` → `Run()` → `delete`. Linked into each game executable |

Depends on: `mge_platform`, `mge_audio`, SDL2_ttf.

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
