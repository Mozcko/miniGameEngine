# Your first game

This guide takes you from nothing to a running game with input, an entity, and a test that runs
without a window, in about 10 minutes. It uses the current engine API: SDL3 window, the SDL3 GPU
API (Vulkan/Metal/D3D12) for drawing, and the ECS. The code shown is what `scripts/new-game.sh`
generates; CI builds that template on every PR, so the guide stays accurate.

Prerequisites: the project builds (`docs/BUILDING.md`).

---

## 1. Create the game

```bash
scripts/new-game.sh space_shooter          # or: --headless for a program without window
cmake --preset dev                         # picks up the new folder
cmake --build --preset dev --target run-space_shooter
```

A window opens whose colour slowly pulses. Esc quits; hold Space to pulse faster.

In VS Code: Terminal → Run Task → **New game**, then Ctrl+Shift+B to build and F5 to debug. See
[BUILDING → VS Code](BUILDING.md#vs-code).

## 2. What was generated

```
examples/space_shooter/
├── CMakeLists.txt     one mge_add_game(...) call
├── SpaceShooter.cpp   the game
└── README.md
```

`CMakeLists.txt`:

```cmake
mge_add_game(space_shooter
    SOURCES SpaceShooter.cpp
    MODULES platform gpu audio  # window + input, GPU, sound. Default: platform gpu graphics audio
    SMOKE_FRAMES 30)            # CTest runs the game 30 frames offscreen
```

`mge_add_game` links the engine's `main()` and the modules, applies the project's warnings, and
creates the `run-space_shooter` target and the smoke test. Options are listed in
[BUILDING → Adding a game](BUILDING.md#adding-a-game).

## 3. The application lifecycle

A game is a subclass of `mge::Application`. One include gives you the engine API:

```cpp
#include <mge/mge.h>   // core + the modules this game links (here: platform, audio)
```

The engine owns `main()` and calls your hooks in this order:

| Hook | When | Use it for |
|------|------|------------|
| constructor | before anything starts | `addModule<…>()`: choose platform, GPU, audio |
| `bool onInit()` | modules are ready (window and GPU device exist) | load things, bind input, create entities. Return `false` to abort (exit code 1) |
| `onFixedUpdate(float fixedDt)` | 0..N times per frame at a fixed 60 Hz | **simulation**: physics, rules. Deterministic |
| `onUpdate(float dt)` | once per frame, scaled time (0 while paused) | input, camera, things that follow the frame rate |
| `onRender()` | once per frame | draw. `time().alpha()` interpolates between fixed steps |
| `onShutdown()` | after the loop, before modules shut down | free GPU resources (the device still exists) |

Stop with `requestStop()`. Pause with `time().setTimeScale(0.0f)`.

```cpp
class SpaceShooter : public mge::Application {
public:
    SpaceShooter() : mge::Application(makeConfig()) {
        addModule<mge::PlatformModule>(); // window and input
        addModule<mge::GpuModule>();      // GPU (Vulkan/Metal/D3D12); after the platform
        addModule<mge::AudioModule>();
    }
    // ...
private:
    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "Space Shooter";
        config.width = 1280;
        config.height = 720;
        return config;
    }
};

mge::Application* mge::createApplication() {
    return new SpaceShooter();
}
```

## 4. Input: actions, not keys

Bind named actions once, then ask about actions. Changing controls only means changing bindings.

```cpp
bool onInit() override {
    m_platform = getModule<mge::PlatformModule>();
    m_platform->actions().bind("Quit", mge::Key::Escape);
    m_platform->actions().bind("Faster", mge::Key::Space);
    m_platform->actions().bindAxis("MoveRight", mge::Key::D, mge::Key::A);   // -1..1
    return true;
}

void onUpdate(float /*dt*/) override {
    if (m_platform->wasActionPressed("Quit")) requestStop();    // only the frame it was pressed
    bool fast = m_platform->isActionDown("Faster");               // held
    float right = m_platform->axis("MoveRight");
}
```

The mouse is available through `m_platform->input().mouseDelta()`; capture it with
`m_platform->setCursorLocked(true)`.

## 5. Entities and systems (ECS)

Components are plain structs. A system is a function that runs in a phase: **Fixed**
(simulation), **Update** or **Render**.

```cpp
struct Pulse {
    float phase = 0.0f;
    float speed = 1.0f;
};

bool onInit() override {
    mge::Registry& registry = world().registry();
    m_pulse = registry.create();
    registry.add<Pulse>(m_pulse);

    world().addSystem(mge::SystemPhase::Fixed, "pulse", [](mge::World& w, float dt) {
        w.registry().each<Pulse>(
            [dt](mge::Entity, Pulse& pulse) { pulse.phase += pulse.speed * dt; });
    });
    return true;
}
```

For positions use the built‑in `mge::Transform` (and `mge::Parent` for hierarchies; see
`src/mge/core/README.md`).

## 6. Drawing

`GpuModule` gives you the frame each time `onRender` runs: a command buffer plus the window's image.
The template only clears it:

```cpp
void onRender() override {
    const mge::GpuFrame* frame = m_gpu->frame();       // m_gpu = getModule<mge::GpuModule>()
    if (!frame) return;                                // minimised window: no image this frame
    const float phase = world().registry().get<Pulse>(m_pulse).phase;
    mge::ColorTarget target;
    target.texture = frame->swapchain;
    target.clearColor = {0.5f + 0.4f * std::sin(phase), 0.25f, 0.5f + 0.4f * std::cos(phase), 1.0f};
    mge::RenderPass pass = frame->cmd->beginRenderPass(target);   // ends at scope exit
}
```

The `GpuModule` submits the frame for you.

**Drawing meshes:**
- **Engine renderer:** link `graphics` and use `mge::MeshRenderer`. Generated or loaded meshes
  (`MeshData` → `device.uploadMesh`), textures (`Texture2D`) and a camera; see
  `src/mge/graphics/README.md` and `game/SandboxApp.cpp`.
- **Your own shaders:** write GLSL and embed it with `mge_add_shaders`. See
  `examples/gpu_triangle` and `src/mge/gpu/README.md`.

## 7. Logging, randomness, assets

```cpp
MGE_LOG_INFO("space_shooter", "score: ", score);         // Trace/Debug are stripped in Release
mge::Pcg32 rng(mge::hash2i(x, y, worldSeed), 1);        // deterministic; never rand()
auto shader = mge::assetPath("shaders/basic3d.vert");    // works from any working directory
```

The engine is **procedural‑first, assets optional** ([ADR 0004](adr/0004-procedural-first-assets-optional.md)):
generate content in code where you can, and load files where they make sense. Both work, and you
can mix them.

## 8. Run, debug, test

```bash
cmake --build --preset dev --target run-space_shooter                # run
MGE_MAX_FRAMES=300 SDL_VIDEODRIVER=offscreen ./build/space_shooter   # no window, exits after 300 frames
cmake --build --preset dev && ctest --preset dev -R space_shooter   # the smoke test (ctest doesn't build)
```

In VS Code, F5 debugs. To debug your own game, copy the "Debug sandbox" entry in
`.vscode/launch.json` and change `program`.

**Testing game logic headless.** Logic that doesn't need a window can run in an `Application`
with **no modules** and a `ManualClock`. Every frame then lasts exactly 1/60 s, so the result is
the same on every machine. That's the `headless` template (`scripts/new-game.sh my_sim --headless`):

```cpp
class MySim : public mge::Application {
public:
    MySim() : mge::Application(makeConfig()) {           // config.headless = true; config.maxFrames = 600
        setClock(std::make_unique<mge::ManualClock>(1.0 / 60.0));
    }
protected:
    void onShutdown() override {
        if (!resultIsCorrect()) setExitCode(1);           // ctest marks the run as failed
    }
};
```

For unit tests of individual classes, add a doctest file under `tests/` (see `tests/README.md`).

---

Next: the engine reference lives in each folder's README (`src/mge/core/README.md` for the
Application, ECS, input and time). The full user guide (#67) will extend this page.
