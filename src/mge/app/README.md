# src/mge/app/  →  targets `mge_app`, `mge_main`

Ties the modules together: creates the window, initialises audio and SDL_ttf, and runs the game
loop. **Temporary home**: in #11 `Application` moves into `mge_core` as a module‑based, headless‑capable
runner, and platform/audio become optional modules.

| File | Target | Purpose |
|------|--------|---------|
| `Application.h/.cpp` | `mge_app` | Base class for games (namespace `mge`) |
| `Main.cpp` | `mge_main` (OBJECT library) | `main()`: `createApplication()` → `run()` → `delete`. Linked into each game executable |

Depends on: `mge_platform`, `mge_audio`, SDL2_ttf.

## `mge::Application`

### Interface
```cpp
class Application {
public:
    Application();
    virtual ~Application();
    void Run();                                  // blocks until m_running == false

    virtual void onInit() = 0;                   // once, after window + input exist
    virtual void onUpdate(float dt) = 0;         // every frame, dt in seconds
    virtual void onRender() = 0;                 // every frame, after onUpdate
    virtual void onEvent(SDL_Event& event) {}    // every SDL event, before InputManager sees it

    mge::Window& getWindow();
protected:
    bool m_running;                              // set false to exit
    std::unique_ptr<mge::Window> m_window;
private:
    void calculateDeltaTime();                   // SDL performance counter based
    float m_deltaTime; Uint64 m_lastFrameTime;
};
Application* createApplication();                // defined by the game
```

### Constructor (in order)
1. `AudioManager::init()`
2. `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)` – on failure sets `m_running = false` and returns
   (the window is then never created → `run()` would dereference null).
3. Creates `Window("Mini Game Engine - Minecraft Cube", 1280, 720)` – title and size are hard‑coded here.
4. `TTF_Init()` (error printed with `printf`, not fatal).

### Destructor
`AudioManager::shutdown()` → `TTF_Quit()` → `SDL_Quit()`; then `m_window` is destroyed.

### `run()`
```
InputManager::init(window) → onInit() → loop:
    calculateDeltaTime → InputManager::update → poll events (SDL_QUIT ends loop; onEvent; InputManager::processEvent)
    → onUpdate(m_deltaTime) → onRender() → m_window->swapBuffers()
```
No frame cap; vsync depends on driver defaults (no `SDL_GL_SetSwapInterval` call). `m_deltaTime`
is **not clamped** (unlike `Time`).
