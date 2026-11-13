# Your first game

This guide takes you from nothing to a running 3D game with input, a character controller, procedural meshes, sky and lighting, an animated entity, and headless unit tests, in about 10 minutes. It uses the current engine API: SDL3 window, SDL3 GPU API (Vulkan/Metal/D3D12), and the ECS.

The code shown is what `scripts/new-game.sh` generates using the **3D template**; CI builds and tests the template on every PR, so this guide stays accurate.

Prerequisites: the project builds (`docs/BUILDING.md`).

> [!NOTE]
> This is the quick 10-minute getting started guide. For the comprehensive manual covering all subsystems, modules, physics, voxels, procedural generation, sound synthesis, and an end-to-end game walkthrough, see the [Full User Guide](GUIDE.md).

> [!TIP]
> Prefer writing your game in C or Python? `miniGameEngine` supports plain C99 (`mge new <name> --lang c`) and Python (`mge new <name> --lang python`) project templates. See the [C API Guide](C_API.md) and [Python Guide](PYTHON.md) for detailed tutorials.

---

## 1. Create the game

```bash
scripts/new-game.sh space_shooter              # uses the 3D template by default
cmake --preset dev                             # picks up the new folder
cmake --build --preset dev --target run-space_shooter
```

A 3D window opens: you are walking on a procedural green ground plane under a procedural sky with dynamic lighting. In front of you are decorative geometric primitives and a bobbing, rotating golden orb.
- **WASD**: walk.
- **Space**: jump (plays a synthesized procedural jump sound).
- **Shift**: sprint.
- **Mouse**: look around (cursor is locked).
- **F1**: toggle the performance overlay (FPS, frame-time graph, profiler scope timings).
- **Esc**: exit.

In VS Code: Terminal → Run Task → **New game**, then Ctrl+Shift+B to build and F5 to debug. See [BUILDING → VS Code](BUILDING.md#vs-code).

## 2. What was generated

The template creates a complete, standalone project:

```
examples/space_shooter/
├── CMakeLists.txt              CMake configuration (find_package + FetchContent fallback)
├── CMakePresets.json           dev (Debug) and release presets
├── README.md                   Build, run and test instructions
├── .gitignore                  Ignores build directories and binaries
├── .github/workflows/ci.yml    Continuous integration workflow
├── src/
│   ├── SpaceShooter.cpp        Application entry point (3D scene, character, audio, UI)
│   ├── SpaceShooterLogic.h     Decoupled headless game logic (pure structs and functions)
│   └── SpaceShooterLogic.cpp
└── tests/
    └── test_space_shooter.cpp  doctest unit tests (runs without window or GPU)
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(space_shooter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Look for installed engine, or fetch automatically
find_package(mge 1.0 QUIET)
if(NOT mge_FOUND AND NOT TARGET mge::core)
    include(FetchContent)
    FetchContent_Declare(mge GIT_REPOSITORY https://github.com/Mozcko/miniGameEngine.git GIT_TAG v1.0.0)
    FetchContent_MakeAvailable(mge)
endif()

# 2. Decoupled game logic library
add_library(space_shooter_logic STATIC
    src/SpaceShooterLogic.cpp
    src/SpaceShooterLogic.h
)
target_include_directories(space_shooter_logic PUBLIC src)
target_link_libraries(space_shooter_logic PUBLIC mge::core mge::physics)

# 3. Game executable
mge_add_game(space_shooter
    SOURCES src/SpaceShooter.cpp
    LIBRARIES space_shooter_logic
    MODULES platform gpu graphics audio physics procgen
    SMOKE_FRAMES 60)

# 4. Headless doctest unit tests
enable_testing()
add_executable(space_shooter_tests tests/test_space_shooter.cpp)
target_link_libraries(space_shooter_tests PRIVATE space_shooter_logic space_shooter_doctest)
add_test(NAME space_shooter_tests COMMAND space_shooter_tests)
```

`mge_add_game` links the engine's `main()` and the modules, applies warnings, creates the `run-space_shooter` target, and registers the 60-frame offscreen smoke test in CTest.

## 3. The application lifecycle

A game is a subclass of `mge::Application`:

```cpp
#include <mge/mge.h>
#include "SpaceShooterLogic.h"

class SpaceShooter : public mge::Application {
public:
    SpaceShooter() : mge::Application(makeConfig()) {
        addModule<mge::PlatformModule>(); // Window and input
        addModule<mge::GpuModule>();      // GPU device (Vulkan/Metal/D3D12)
        addModule<mge::AudioModule>();    // Synthesizer and mixer
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

The engine owns `main()` and calls your hooks in this order:

| Hook | When | Use it for |
|------|------|------------|
| constructor | before anything starts | `addModule<…>()`: choose platform, GPU, audio |
| `bool onInit()` | modules are ready (window and GPU device exist) | load/upload meshes, bind input, create entities. Return `false` to abort (exit code 1) |
| `onFixedUpdate(float fixedDt)` | 0..N times per frame at a fixed 60 Hz | **simulation**: physics, character stepping, rules. Deterministic |
| `onUpdate(float dt)` | once per frame, scaled time (0 while paused) | input, mouse look, animations, camera updates |
| `onRender()` | once per frame | draw 3D scene, sky, lighting, 2D HUD. `time().alpha()` interpolates between fixed steps |
| `onShutdown()` | after the loop, before modules shut down | free GPU resources (meshes, renderers) |

Stop with `requestStop()`. Pause with `time().setTimeScale(0.0f)`.

## 4. Input: actions, not keys

Bind named actions once, then query actions in updates:

```cpp
bool onInit() override {
    auto& actions = m_platform->actions();
    actions.bindAxis("MoveX", mge::Key::D, mge::Key::A);      // -1..1
    actions.bindAxis("MoveZ", mge::Key::W, mge::Key::S);      // -1..1
    actions.bind("Jump", mge::Key::Space);
    actions.bind("Sprint", mge::Key::LeftShift);
    actions.bind("Overlay", mge::Key::F1);
    actions.bind("Quit", mge::Key::Escape);

    m_platform->setCursorLocked(true);
    return true;
}

void onUpdate(float dt) override {
    if (m_platform->wasActionPressed("Quit")) requestStop();
    if (m_platform->wasActionPressed("Overlay")) m_overlay.toggle();

    auto& controller = world().registry().get<mge::CharacterController>(m_playerEntity);
    if (m_platform->isCursorLocked()) {
        const mge::Vec2f mouseDelta = m_platform->input().mouseDelta();
        controller.look(mouseDelta.x * 0.15f, -mouseDelta.y * 0.15f);
    }
    controller.input.moveForward = m_platform->axis("MoveZ");
    controller.input.moveRight = m_platform->axis("MoveX");
    controller.input.sprint = m_platform->isActionDown("Sprint");
}
```

## 5. Procedural 3D content and physics

The engine is **procedural-first, assets optional** ([ADR 0004](adr/0004-procedural-first-assets-optional.md)). Everything in the template is generated in code without external files:

```cpp
// 1. Procedural meshes generated with mge::primitives
mge::GpuDevice& device = m_gpu->device();
m_floorMesh = device.uploadMesh(
    mge::primitives::plane(50.0f, 50.0f, 10, 10, glm::vec4(0.28f, 0.42f, 0.25f, 1.0f)));
m_boxMesh = device.uploadMesh(
    mge::primitives::box(glm::vec3(1.2f), glm::vec4(0.70f, 0.45f, 0.25f, 1.0f)));
m_orbMesh = device.uploadMesh(
    mge::primitives::uvSphere(0.5f, 24, 16, glm::vec4(0.95f, 0.80f, 0.20f, 1.0f)));

// 2. Synthesized procedural sound effect (no wav/mp3 required)
m_jumpSfx = m_audio->addSfx(mge::sfx::jump());

// 3. Character controller on a flat SolidQuery floor (y < 0 is solid ground)
m_playerEntity = world().registry().create();
mge::CharacterController playerController;
world().registry().add<mge::Transform>(m_playerEntity, glm::vec3(0.0f, 0.0f, 0.0f));
world().registry().add<mge::CharacterController>(m_playerEntity, playerController);

m_cameraEntity = world().registry().create();
world().registry().add<mge::Transform>(m_cameraEntity);
world().registry().add<mge::Camera>(m_cameraEntity);
world().registry().add<mge::CharacterCamera>(m_cameraEntity, mge::CharacterCamera{m_playerEntity});

mge::addCharacterSystem(world(), space_shooter::isFloorSolid);
mge::addCharacterCameraSystem(world(), time());
```

## 6. Entities and systems (ECS)

In `SpaceShooterLogic.h`:
```cpp
struct FloatingOrb {
    glm::vec3 basePosition{0.0f, 1.5f, -5.0f};
    float phase = 0.0f;
    float bobSpeed = 2.5f;
    float bobAmplitude = 0.4f;
    float rotationSpeed = 1.8f;
    float rotationAngle = 0.0f;
};
void updateFloatingOrb(FloatingOrb& orb, float dt);
glm::vec3 computeOrbPosition(const FloatingOrb& orb);
```

In `onInit()`, attach it to an entity and register a system running in `SystemPhase::Fixed`:
```cpp
m_orbEntity = world().registry().create();
world().registry().add<mge::Transform>(m_orbEntity, glm::vec3(0.0f, 1.5f, -5.0f));
world().registry().add<space_shooter::FloatingOrb>(m_orbEntity);

world().addSystem(mge::SystemPhase::Fixed, "floating_orb", [](mge::World& w, float dt) {
    w.registry().each<space_shooter::FloatingOrb, mge::Transform>(
        [dt](mge::Entity, space_shooter::FloatingOrb& orb, mge::Transform& transform) {
            space_shooter::updateFloatingOrb(orb, dt);
            transform.position = space_shooter::computeOrbPosition(orb);
            transform.rotation = glm::angleAxis(orb.rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        });
});
```

## 7. Drawing 3D and 2D

`MeshRenderer` draws 3D geometry; `SkyRenderer` renders the procedural sky; `Renderer2D` draws pixel-space HUD and the debug overlay on top:

```cpp
void onRender() override {
    const mge::GpuFrame* frame = m_gpu->frame();
    if (!frame) return;

    const auto& cameraTransform = world().registry().get<mge::Transform>(m_cameraEntity);
    const auto& camera = world().registry().get<mge::Camera>(m_cameraEntity);
    const mge::CameraMatrices mats = mge::cameraMatrices(cameraTransform.localMatrix(), camera, viewport());

    const mge::Lighting currentLighting = m_dayNight.lighting(m_lighting);

    if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                          glm::vec4(currentLighting.fogColor, 1.0f))) {
        m_renderer->setCamera(mats.viewProjection, mats.position);
        m_renderer->setLighting(currentLighting);

        m_skyRenderer->draw(*m_renderer, glm::inverse(mats.viewProjection), mats.position, m_dayNight.sky());

        // Draw ground and objects
        m_renderer->draw(m_floorMesh, glm::mat4(1.0f));
        m_renderer->draw(m_boxMesh, glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, 0.6f, -6.0f)));

        const auto& orbTransform = world().registry().get<mge::Transform>(m_orbEntity);
        m_renderer->draw(m_orbMesh, orbTransform.localMatrix());

        m_renderer->end();
    }

    // 2D HUD and performance overlay
    m_overlay.addFrame(time().unscaledDelta() * 1000.0f, frame->acquireMs);
    m_overlay.setProfile(profiler());
    m_ui->begin(frame->width, frame->height, std::max(1.0f, std::round(frame->displayScale)));
    m_ui->text({16.0f, m_ui->size().y - 40.0f}, "Space Shooter", {1.0f, 1.0f, 1.0f, 1.0f}, 2.5f);
    m_overlay.draw(*m_ui);
    m_ui->flush(*frame->cmd, frame->swapchain);
}
```

## 8. Run, debug, test

```bash
cmake --build --preset dev --target run-space_shooter              # Run game
ctest --preset dev                                                # Runs unit tests and 60-frame offscreen smoke test
```

**Testing game logic headless:**
The game logic lives in `space_shooter_logic` and is tested in `tests/test_space_shooter.cpp` without any window or GPU:

```cpp
TEST_CASE("space_shooter: oscilacion de orbe flotante") {
    FloatingOrb orb;
    for (int i = 0; i < 60; ++i) {
        updateFloatingOrb(orb, 1.0f / 60.0f);
        const glm::vec3 pos = computeOrbPosition(orb);
        CHECK(pos.y >= 1.0f);
        CHECK(pos.y <= 2.0f);
    }
}
```

---

Next:
- See other project templates in [`templates/README.md`](../templates/README.md) (`2d`, `headless`).
- Check the [Full User Guide](GUIDE.md) covering all engine modules, voxel streaming, and synthesis.
