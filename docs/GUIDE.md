# miniGameEngine User Guide

Welcome to the comprehensive **miniGameEngine (MGE)** user guide. This document covers the architecture, subsystems, and workflows for building games with the engine.

MGE is designed around three architectural pillars:
1. **Procedural-first, assets optional** ([ADR 0004](adr/0004-procedural-first-assets-optional.md)): Every subsystem provides a mathematical, code-driven generation path. An entire game can compile into a single self-contained binary with zero external asset files. When you want external PNGs, TTFs, or WAVs, clean loaders are readily available.
2. **Headless core & modules** ([ADR 0003](adr/0003-headless-core.md)): Core game logic, ECS, physics, voxel worlds, procedural math, and sound synthesis depend solely on the standard library and GLM math. They run and test headless without a window, display server, or GPU. Windowing, GPU rendering, and device audio streaming live in modular layers.
3. **Modern SDL3 GPU graphics** ([ADR 0001](adr/0001-sdl3-gpu-over-raw-vulkan.md)): High-level 3D mesh rendering, procedural sky, SDF raymarching, and 2D immediate UI built on SDL3's cross-platform GPU abstraction (Vulkan, Metal, Direct3D 12) with compile-time embedded SPIR-V shaders.

> [!TIP]
> Looking for a quick 10-minute setup? See [Your First Game](FIRST_GAME.md) for a concise jump-start tutorial.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Modules & the Headless Core](#2-modules--the-headless-core)
3. [Application Loop & Time](#3-application-loop--time)
4. [Input Actions](#4-input-actions)
5. [Entities & Systems (ECS)](#5-entities--systems-ecs)
6. [Procedural Generation (Procgen)](#6-procedural-generation-procgen)
7. [Voxel Worlds](#7-voxel-worlds)
8. [Rendering](#8-rendering)
9. [Physics & Collisions](#9-physics--collisions)
10. [Synthesized Audio](#10-synthesized-audio)
11. [Profiling & Debugging](#11-profiling--debugging)
12. [Assets: Code vs Files](#12-assets-code-vs-files)
13. [Step-by-Step Tutorial: Building a Code-Only Game](#13-step-by-step-tutorial-building-a-code-only-game)

---

## 1. Getting Started

### System Dependencies

MGE requires a C++17 compiler, CMake 3.21+, and Vulkan graphics drivers:

- **Linux (Fedora)**:
  ```bash
  sudo dnf install gcc-c++ cmake ninja-build SDL3-devel glslang mesa-vulkan-drivers mesa-libGL-devel
  ```
- **Linux (Ubuntu 24.04)**:
  ```bash
  sudo apt-get install build-essential cmake ninja-build glslang-tools libvulkan-dev libasound2-dev libx11-dev libxcursor-dev libxi-dev libxrandr-dev libxinerama-dev libwayland-dev
  ```
  *(On Ubuntu, SDL3 is automatically fetched and built statically from source via CMake FetchContent).*
- **Headless / CI / Virtual Machines**:
  Mesa Lavapipe provides CPU Vulkan rendering for offscreen execution:
  ```bash
  # Linux package: mesa-vulkan-drivers (Ubuntu) or mesa-dri-drivers (Fedora)
  export VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
  ```

### CMake Presets

The project uses `CMakePresets.json` to standardize build profiles:

| Preset | Build Type | Features | Primary Use |
| :--- | :--- | :--- | :--- |
| `dev` | Debug | Full warnings enabled | Local iterative development |
| `release` | Release | Optimized (`-O3`), asserts stripped | Production releases and benchmarks |
| `ci` | Debug | Warnings treated as errors (`-Werror`) | Pre-commit validation and CI builds |
| `asan` | Debug | AddressSanitizer + UndefinedBehaviorSanitizer | Memory corruption and UB detection |
| `tsan` | Debug | ThreadSanitizer (headless modules) | Data race verification |
| `minsize` | MinSizeRel | Size optimized (`-Os`), LTO/IPO, stripped | Binary size budget enforcement |

Configure and build with CMake:
```bash
cmake --preset dev
cmake --build --preset dev
```

### Creating Games with `scripts/new-game.sh`

MGE provides a project generator script that sets up new games in `examples/<name>`:

```bash
# Standard windowed game with Platform, GPU, Graphics, and Audio modules
scripts/new-game.sh my_game

# Headless simulation without window, GPU, or SDL (pure logic, tests, or servers)
scripts/new-game.sh my_sim --headless
```

Reconfigure and run your new game in one step:
```bash
cmake --preset dev
cmake --build --preset dev --target run-my_game
```

---

## 2. Modules & the Headless Core

MGE cleanly separates headless mathematical algorithms from OS and hardware interfaces.

```
┌────────────────────────────────────────────────────────┐
│                      game / executable                 │
├──────────────────────────┬─────────────────────────────┤
│   Hardware Modules       │   Headless Engine Modules   │
│   (SDL3 Window & GPU)    │   (Pure C++17 & GLM math)   │
├──────────────────────────┼─────────────────────────────┤
│  mge_platform (Window)   │  mge_core (App, ECS, Time)  │
│  mge_gpu (SDL3 GPU)      │  mge_procgen (Mesh, Noise)  │
│  mge_graphics (Renderer) │  mge_voxel (Chunks, Mesher) │
│  mge_audio (Audio stream)│  mge_synth (Mixer, Synthesizer)
│                          │  mge_physics (Swept AABB)   │
└──────────────────────────┴─────────────────────────────┘
```

### Headless Libraries (Always Available)

These libraries compile even when `MGE_BUILD_MODULES=OFF` on systems without windowing or graphics libraries:
- **`mge_core`**: Application runner, module system, Entity-Component-System (ECS), input maps, logging, math geometry (`Aabb`, `Ray`, `Plane`), time management, and PCG32 random generators.
- **`mge_procgen`**: CPU mesh data representation (`MeshData`), procedural 3D primitives, mesh operations (welding, normal calculation, extrusions, lathe), procedural noise, and Signed Distance Fields (SDF).
- **`mge_voxel`**: Block definitions, chunk data storage, voxel terrain generation, multi-threaded chunk meshing, and fast DDA voxel raycasting.
- **`mge_synth`**: Lock-free procedural audio mixer, PolyBLEP synthesizer oscillators, ADSR envelopes, resonant filters, sfxr-style sound parameters, and tracker song sequencer.
- **`mge_physics`**: Continuous swept AABB voxel collision, 3D uniform spatial hashing, character controller simulation, and rigid body physics.

### Hardware Modules (Optional Additions)

Games register hardware modules during `Application` construction:
```cpp
#include <mge/mge.h>

class MyGame : public mge::Application {
public:
    MyGame() : mge::Application(makeConfig()) {
        addModule<mge::PlatformModule>(); // SDL3 window creation & event polling
        addModule<mge::GpuModule>();      // SDL3 GPU device & swapchain
        addModule<mge::AudioModule>();    // Audio callback stream for mge_synth
    }
};
```

---

## 3. Application Loop & Time

The engine controls the main loop and coordinates timesteps across modules and game code.

### Application Lifecycle Hooks

| Hook | Timestep | Purpose |
| :--- | :--- | :--- |
| `constructor` | Pre-init | Call `addModule<T>()` to select required modules. |
| `bool onInit()` | Once | Modules are initialized; window and GPU device exist. Return `false` to abort. |
| `onFixedUpdate(fixedDt)`| Fixed 60 Hz | Deterministic physics simulation, movement, and gameplay rules. |
| `onUpdate(dt)` | Variable | Frame-dependent logic: camera smoothing, input sampling, UI animations. |
| `onRender()` | Variable | Drawing commands to `MeshRenderer`, `SdfRenderer`, and `Renderer2D`. |
| `onShutdown()` | Once | Release GPU resources before `GpuModule` shuts down the GPU device. |

### Timestep Control & Time Queries

Access time state using `time()`:
```cpp
// Scaled delta time (seconds elapsed since last frame; 0 when paused)
float dt = time().delta();

// Real wall-clock delta time (unaffected by pause, useful for UI animations)
float unscaledDt = time().unscaledDelta();

// Fixed timestep duration (default 1/60 s)
float fixedDt = time().fixedDelta();

// Interpolation fraction [0, 1) between the previous and current fixed physics steps
float alpha = time().alpha();

// Total elapsed time in seconds
double total = time().elapsed();

// Pause or slow motion
time().setTimeScale(0.0f); // Paused
time().setTimeScale(0.5f); // Half speed
time().setTimeScale(1.0f); // Normal speed
```

### Deterministic Headless Testing

For tests and simulations that must yield identical results across all machines, use `mge::ManualClock`:
```cpp
class HeadlessSim : public mge::Application {
public:
    HeadlessSim() : mge::Application(makeConfig()) {
        // Runs at exactly 60 steps per second with zero drift or OS timer jitter
        setClock(std::make_unique<mge::ManualClock>(1.0 / 60.0));
    }
};
```

---

## 4. Input Actions

MGE decouples physical hardware inputs (keys, mouse buttons) from game logic through **Action Maps**.

### Binding Actions and Axes

Bind named actions once in `onInit()`:
```cpp
auto& actions = m_platform->actions();

// Digital actions (multiple keys can map to the same action)
actions.bind("Jump", mge::Key::Space);
actions.bind("Jump", mge::MouseButton::Right);
actions.bind("Quit", mge::Key::Escape);

// 1D Axis bindings (positive key, negative key -> returns -1.0 .. +1.0)
actions.bindAxis("MoveX", mge::Key::D, mge::Key::A);
actions.bindAxis("MoveZ", mge::Key::S, mge::Key::W);
```

### Querying Actions

Query state during `onUpdate()` or `onFixedUpdate()`:
```cpp
// Check if an action is currently held down
if (m_platform->isActionDown("Jump")) { /* ... */ }

// Check if an action was pressed this exact frame (rising edge)
if (m_platform->wasActionPressed("Jump")) { /* ... */ }

// Check if an action was released this frame (falling edge)
if (m_platform->wasActionReleased("Jump")) { /* ... */ }

// Query axis value in range [-1.0, 1.0]
float moveX = m_platform->axis("MoveX");
float moveZ = m_platform->axis("MoveZ");
```

### Mouse Look & Pointer Locking

Capture and lock the mouse cursor for first-person or flight cameras:
```cpp
// Lock the mouse to the center of the window
m_platform->setCursorLocked(true);

// Read relative delta movement in pixels accumulated this frame
mge::Vec2f mouseDelta = m_platform->input().mouseDelta();
float lookYaw   = mouseDelta.x * 0.002f;
float lookPitch = mouseDelta.y * 0.002f;
```

---

## 5. Entities & Systems (ECS)

MGE includes a high-performance, lightweight Entity-Component-System (ECS) based on sparse sets.

### Entities and Components

Components are plain C++ data structs. Entities are lightweight handles:

```cpp
struct Velocity {
    glm::vec3 value{0.0f};
};

struct Health {
    int current = 100;
    int max = 100;
};

// Creating an entity and attaching components
mge::Registry& registry = world().registry();
mge::Entity player = registry.create();

registry.add<mge::Transform>(player, glm::vec3(0.0f, 1.0f, 0.0f));
registry.add<Velocity>(player, glm::vec3(0.0f, 0.0f, 5.0f));
registry.add<Health>(player, 100, 100);

// Querying components
if (registry.has<Health>(player)) {
    Health& hp = registry.get<Health>(player);
    hp.current -= 10;
}
```

### Iteration & Multi-Component Views

Iterate over all entities with specific components:
```cpp
// Iterate over a single component
registry.each<Health>([](mge::Entity e, Health& hp) {
    if (hp.current <= 0) { /* handle death */ }
});

// Multi-component view
registry.view<mge::Transform, Velocity>().each(
    [](mge::Entity e, mge::Transform& transform, Velocity& vel) {
        transform.position += vel.value * (1.0f / 60.0f);
    });
```

### System Phases

Register systems to execute automatically in designated phases:
```cpp
// Fixed simulation phase (runs inside onFixedUpdate at 60 Hz)
world().addSystem(mge::SystemPhase::Fixed, "movement", [](mge::World& w, float dt) {
    w.registry().view<mge::Transform, Velocity>().each(
        [dt](mge::Entity, mge::Transform& t, Velocity& v) {
            t.position += v.value * dt;
        });
});

// Update phase (runs every frame)
world().addSystem(mge::SystemPhase::Update, "decay", [](mge::World& w, float dt) {
    // Frame-rate dependent animations, timers, particle lifespans
});
```

### Hierarchical Transforms

Entities can form parent-child hierarchies using `mge::Parent` and `mge::WorldTransform`:
```cpp
mge::Entity parent = registry.create();
registry.add<mge::Transform>(parent, glm::vec3(10.0f, 0.0f, 0.0f));

mge::Entity child = registry.create();
registry.add<mge::Transform>(child, glm::vec3(0.0f, 2.0f, 0.0f));
registry.add<mge::Parent>(child, parent);

// TransformSystem computes world space matrices for all children
mge::TransformSystem::update(registry);
glm::vec3 worldPos = registry.get<mge::WorldTransform>(child).position; // (10, 2, 0)
```

---

## 6. Procedural Generation (Procgen)

The `mge_procgen` library provides procedural geometry, noise functions, and signed distance fields.

### `MeshData`

`MeshData` stores geometric vertices in Structure-of-Arrays (SoA) format with positions, normals, colors, UVs, and triangle indices:

```cpp
mge::MeshData mesh;
uint32_t v0 = mesh.addVertex({-1, 0, -1}, {0, 1, 0}, {1, 1, 1, 1}, {0, 0});
uint32_t v1 = mesh.addVertex({ 1, 0, -1}, {0, 1, 0}, {1, 1, 1, 1}, {1, 0});
uint32_t v2 = mesh.addVertex({ 1, 0,  1}, {0, 1, 0}, {1, 1, 1, 1}, {1, 1});
uint32_t v3 = mesh.addVertex({-1, 0,  1}, {0, 1, 0}, {1, 1, 1, 1}, {0, 1});
mesh.addQuad(v0, v1, v2, v3);

// Validation and debug export
std::string error = mesh.validate(); // Returns empty string when valid
mge::writeObj(mesh, "model.obj");    // Export directly to Wavefront OBJ
```

### 3D Primitives (`<mge/procgen/Primitives.h>`)

Watertight procedural primitives centered at the origin:
```cpp
// Plane in XZ, facing +Y
mge::MeshData ground = mge::primitives::plane(20.0f, 20.0f, 4, 4);

// Box with flat faces
mge::MeshData cube = mge::primitives::box(glm::vec3(1.0f));

// UV Sphere (latitude/longitude rings)
mge::MeshData sphere = mge::primitives::uvSphere(0.5f, 32, 16);

// Geodesic Icosphere (subdivided icosahedron, uniform vertices)
mge::MeshData ico = mge::primitives::icosphere(0.5f, 2);

// Cylinder, Cone, Torus, Capsule
mge::MeshData cyl = mge::primitives::cylinder(0.5f, 2.0f, 32);
mge::MeshData cone = mge::primitives::cone(0.5f, 1.5f, 32);
mge::MeshData ring = mge::primitives::torus(1.5f, 0.3f, 32, 16);
mge::MeshData capsule = mge::primitives::capsule(0.4f, 1.2f, 32, 8);
```

### Mesh Operations (`<mge/procgen/MeshOps.h>`)

Transform, combine, and manipulate meshes in memory:
- `transformMesh(mesh, matrix)`: Transform positions and normals.
- `mergeMeshes(meshA, meshB)`: Concatenate two meshes with index offsetting.
- `weldVertices(mesh, epsilon)`: Merge duplicate vertices while preserving UV seams.
- `recomputeNormals(mesh, NormalMode::Smooth | Flat)`: Area-weighted normal recalculation.
- `extrude(polygon2D, depth)`: Extrude 2D polygons into watertight 3D volumes.
- `lathe(profile2D, segments)`: Revolve a 2D curve around the Y-axis.

### Coherent Noise (`<mge/procgen/Noise.h>`)

Deterministic 2D and 3D noise functions seeded with `mge::Pcg32`:
```cpp
mge::Noise noise(1337);

// Basic coherent noise [-1, 1]
float v = noise.perlin(x * 0.05f, z * 0.05f);
float s = noise.simplex(x, y, z);
float w = noise.worley(x, z); // Cellular / Voronoi distance

// Fractal Brownian Motion (fBm)
float elevation = noise.fbm(x * 0.01f, z * 0.01f, /*octaves=*/5, /*gain=*/0.5f, /*lacunarity=*/2.0f);
```

### Signed Distance Fields (`<mge/procgen/Sdf.h>`)

Evaluate signed mathematical shapes ($d < 0$ inside, $d = 0$ surface, $d > 0$ outside):
```cpp
auto sphere = mge::sdf::sphere(1.0f);
auto box = mge::sdf::box(glm::vec3(0.8f));

// Smooth boolean union (k = blend radius)
auto organic = mge::sdf::opSmoothUnion(sphere, box, 0.4f);

// Surface Nets: generate a 3D polygonal mesh from an SDF
mge::Aabb bounds({-2, -2, -2}, {2, 2, 2});
mge::MeshData mesh = mge::sdf::surfaceNets(organic, bounds, /*resolution=*/48);
```

---

## 7. Voxel Worlds

The `mge_voxel` library provides a high-performance voxel engine for infinite, destructible block worlds.

### Block Registry & Shapes

Register block types with render layers and shapes:
```cpp
mge::BlockRegistry registry;

// 1. Solid opaque block with vertex colors
mge::BlockDesc stone;
stone.name = "stone";
stone.color = glm::vec4(0.5f, 0.5f, 0.52f, 1.0f);
stone.solid = true;
stone.opaque = true;
stone.shape = mge::BlockShape::Cube;
mge::BlockId stoneId = registry.add(stone);

// 2. Cutout block (leaves / plants with alpha testing)
mge::BlockDesc flower;
flower.name = "flower";
flower.color = glm::vec4(1.0f, 0.2f, 0.3f, 1.0f);
flower.solid = false;
flower.opaque = false;
flower.layer = mge::RenderLayer::Cutout;
flower.shape = mge::BlockShape::Cross; // Two intersecting diagonal quads
mge::BlockId flowerId = registry.add(flower);
```

### Voxel World & Terrain Generation

```cpp
class ProceduralTerrain : public mge::TerrainGenerator {
public:
    void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override {
        // Generates 16x16x16 blocks statelessly from coordinate
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                chunk.set(x, 0, z, 1); // Place base stone
            }
        }
    }
};

mge::VoxelWorld world(registry, std::make_unique<ProceduralTerrain>());
world.loadChunk({0, 0, 0});
world.setBlock({5, 2, 8}, stoneId); // Sets block in world space, marks chunk dirty
```

### Multi-Threaded Chunk Meshing & Streaming

- **Chunk Meshing**: `buildChunkMesh` inspects neighbor faces, skips hidden internal geometry, and separates triangles into `opaque`, `cutout`, and `transparent` sub-meshes.
- **Thread Safety**: Workers capture immutable `ChunkSnapshot` structs (18³ block IDs including a 1-block perimeter) and mesh concurrently on `JobSystem` background threads without locking `VoxelWorld`.
- **`ChunkStreamer`**: Automatically streams chunks in a sphere or box around the player's position, meshing new chunks and unloading distant ones.

### DDA Voxel Raycasting

Cast rays to detect targeted blocks for picking, breaking, and placing:
```cpp
mge::Ray ray(cameraPosition, cameraForward);
float maxReach = 6.0f;

auto hit = mge::raycastVoxels(world, ray, maxReach);
if (hit.has_value()) {
    glm::ivec3 targetBlock = hit->block;
    glm::ivec3 placePos = hit->block + hit->normal;

    // Break block
    world.setBlock(targetBlock, mge::kAir);
}
```

---

## 8. Rendering

MGE's rendering pipeline is built on SDL3 GPU with clean RAII handles and compile-time embedded SPIR-V bytecode.

### MeshRenderer & Three-Pass Geometry

`MeshRenderer` draws 3D geometry in three distinct passes:
1. **`MeshPass::Opaque`**: Depth testing on (`LESS`), depth write enabled, no alpha blending. Front-to-back sorting for early-Z hardware rejection.
2. **`MeshPass::Cutout`**: Depth testing on, depth write enabled, fragment shader discards pixels with alpha $< 0.5$ (e.g. foliage, fences).
3. **`MeshPass::Transparent`**: Depth testing on, depth write disabled, standard alpha blending (`SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`). Back-to-front sorting.

```cpp
m_renderer = std::make_unique<mge::MeshRenderer>(device, gpuModule->swapchainFormat());
mge::GpuMesh cubeMesh = device.uploadMesh(mge::primitives::box(glm::vec3(1.0f)));

void onRender() override {
    const mge::GpuFrame* frame = m_gpu->frame();
    if (!frame) return;

    if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height, skyColor)) {
        m_renderer->setCamera(projectionMatrix * viewMatrix, cameraPosition);
        m_renderer->setLighting(m_lighting);

        // Batch rendering via RenderQueue
        m_queue.clear();
        m_queue.add({&cubeMesh, modelMatrix, nullptr, mge::MeshPass::Opaque, worldCenter});
        m_queue.sort(cameraPosition, cameraForward);
        m_renderer->submit(m_queue);

        m_renderer->end();
    }
}
```

### Lighting & Atmospheric Fog

Set scene lighting through `mge::Lighting`:
```cpp
mge::Lighting lighting;
lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
lighting.sunColor = glm::vec3(1.0f, 0.95f, 0.85f);
lighting.skyAmbient = glm::vec3(0.40f, 0.45f, 0.55f);
lighting.groundAmbient = glm::vec3(0.20f, 0.18f, 0.16f);

// Linear distance fog
lighting.fog = true;
lighting.fogColor = glm::vec3(0.5f, 0.6f, 0.7f);
lighting.fogStart = 40.0f;
lighting.fogEnd = 160.0f;

m_renderer->setLighting(lighting);
```

### Procedural Sky & Day/Night Cycle

- **`SkyRenderer`**: Draws a procedural sky gradient with directional sun disc and twinkling night stars on a fullscreen triangle before opaque geometry.
- **`DayNight`**: Advances continuous game time (0..24 hours) and interpolates zenith, horizon, ground colors, sun position, and lighting parameters across keyframes (dawn, noon, sunset, night).

### SDF Raymarching Pass (`SdfRenderer`)

`SdfRenderer` renders mathematical signed distance fields directly in screen space using a custom fragment shader that includes `mge/sdf_pass.glsl`. Because the pass performs depth testing and writes `gl_FragDepth`, SDF shapes accurately intersect rasterized meshes in the scene.

### 2D Immediate UI & Text (`Renderer2D`)

Draw UI overlays, HUDs, rects, and text in logical pixel coordinates:
```cpp
m_ui->begin(frame->width, frame->height, /*displayScale=*/1.0f);

// Solid and outlined rectangles
m_ui->rect({20.0f, 20.0f}, {200.0f, 30.0f}, glm::vec4(0.1f, 0.1f, 0.1f, 0.8f));
m_ui->rectOutline({20.0f, 20.0f}, {200.0f, 30.0f}, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

// Text using the embedded 8x8 bitmap font (no external font file needed)
m_ui->text({25.0f, 26.0f}, "Score: 1200", glm::vec4(1.0f, 1.0f, 0.2f, 1.0f), /*scale=*/2.0f);

m_ui->flush(*frame->cmd, frame->swapchain);
```

---

## 9. Physics & Collisions

The `mge_physics` library provides continuous voxel collision resolution, character movement, and spatial broadphase queries.

### Swept AABB Collision against Voxels

`moveAabb` performs continuous collision testing along Y, then X, then Z, preventing tunneling through walls even at high velocities:
```cpp
mge::MoveOptions options;
options.stepHeight = 0.6f; // Automatically step over obstacles up to 0.6 units high
options.skin = 1e-4f;

mge::MoveResult res = mge::moveAabb(world, entityAabb, velocity * dt, options);
entityAabb = res.box;
if (res.grounded) {
    velocity.y = 0.0f; // Landed on ground
}
```

### Character Controller (`<mge/physics/CharacterController.h>`)

Simulate player movement with gravity, ground detection, and auto-stepping:
- **`CharacterMode::Walk`**: Planar acceleration along camera forward/right vectors, gravity, jump impulse when grounded, and air drag.
- **`CharacterMode::Fly`**: 3D omnidirectional velocity with collision checks.
- **`CharacterCamera`**: Smoothly aligns camera orientation and interpolates position using `time().alpha()` to eliminate render jitter.

### Spatial Hash Broadphase (`<mge/physics/SpatialHash.h>`)

3D uniform-grid spatial hash for fast $O(1)$ overlap queries:
```cpp
mge::SpatialHash grid(/*cellSize=*/2.0f);
grid.clear();

for (auto& entity : entities) {
    grid.insert(entity.id, entity.bounds);
}

// Find all unique overlapping pairs
std::vector<std::pair<uint32_t, uint32_t>> pairs;
grid.computePairs(pairs);

// Query all entities overlapping an AABB volume
std::vector<uint32_t> hitIds;
grid.query(blastAabb, hitIds);
```

### Rigid Body Dynamics (`<mge/physics/RigidBody.h>`)

Basic rigid body components (`ShapeType::Box` and `ShapeType::Sphere`) simulated against voxels and dynamic bodies with restitution, Coulomb friction, and Baumgarte contact stabilization.

---

## 10. Synthesized Audio

The `mge_synth` and `mge_audio` modules provide zero-allocation, thread-safe procedural audio.

### The Lock-Free Audio Mixer

Commands sent from the main game thread (`play`, `playNote`, `setBusVolume`) are dispatched through a lock-free Single-Producer Single-Consumer (`SpscQueue`) ring buffer. The audio callback thread renders voices directly into the hardware buffer without memory allocations or mutex locks.

### Procedural Sound Effects (sfxr-Style)

Sound effects are data parameters rendered once into in-memory PCM buffers:
```cpp
// Add synthesized preset sound effects
mge::SampleId coinSfx = m_audio->addSfx(mge::sfx::coin());
mge::SampleId jumpSfx = m_audio->addSfx(mge::sfx::jump());
mge::SampleId hurtSfx = m_audio->addSfx(mge::sfx::hurt());

// Play sound with gain and pan
m_audio->play(coinSfx, /*gain=*/0.8f, /*pan=*/0.0f);

// Create custom procedural sounds with sfx::mutate or randomize
mge::SfxParams laser = mge::sfx::randomize(/*seed=*/1337);
mge::SfxParams variant = mge::sfx::mutate(mge::sfx::jump(), /*mutationAmount=*/0.1f, /*seed=*/42);
```

### Procedural Music Sequencer

Compose electronic tracker-style songs directly in C++:
```cpp
mge::Song song(/*bpm=*/128.0f, /*stepsPerBeat=*/4);

// Define instruments
mge::Instrument lead;
lead.wave = mge::Waveform::Square;
lead.pulseWidth = 0.35f;
lead.envelope = {0.005f, 0.06f, 0.4f, 0.12f}; // ADSR

mge::Instrument bass;
bass.wave = mge::Waveform::Saw;
bass.envelope = {0.005f, 0.08f, 0.5f, 0.08f};

int iLead = song.addInstrument(lead);
int iBass = song.addInstrument(bass);

// Tracker pattern notation: note, '.' silence, '-' sustain, '|' bar separator
int patLead = song.pattern("C4 . E4 . | G4 - E4 . | A4 . G4 . | E4 - - . ");
int patBass = song.pattern("C3 . C3 . | E3 . E3 . | F3 . F3 . | G3 . G3 . ");

song.addTrack(iLead, {patLead, patLead});
song.addTrack(iBass, {patBass, patBass});

mge::SongId songId = m_audio->addSong(song);
m_audio->playSong(songId, /*gain=*/0.6f);
```

---

## 11. Profiling & Debugging

MGE provides built-in instrumentation to monitor CPU frame times and identify bottlenecks.

### Profiler Scopes

Instrument code blocks using `MGE_PROFILE_SCOPE`:
```cpp
void simulateWorld() {
    MGE_PROFILE_SCOPE("WorldSimulation");
    // Heavy calculations...
}
```

### In-Game Performance Overlay (`DebugOverlay`)

Toggle the debug overlay (FPS, frame-time graph, and custom counters) using `F1`:
```cpp
m_overlay.setCounter("Entities", std::to_string(entityCount));
m_overlay.setCounter("Chunks Loaded", std::to_string(chunkCount));

// Render overlay during onRender()
m_overlay.draw(*m_ui);
```

---

## 12. Assets: Code vs Files

MGE follows [ADR 0004](adr/0004-procedural-first-assets-optional.md): **procedural-first, assets optional**.

### Comparison Table

| Subsystem | Code-Driven Path (In-Memory) | File-Driven Path (External Assets) |
| :--- | :--- | :--- |
| **Textures** | `Texture2D::fromPixels`, `mge::texgen::*` | `Texture2D::fromFile("textures/atlas.png")` |
| **Fonts** | `Font::builtin()` (embedded 8x8 font) | `loadTtfFontFile("fonts/font.ttf", px)` |
| **Meshes** | `mge::primitives::*`, `mge::sdf::*` | Procedural only (export with `writeObj`) |
| **Audio** | `mge::sfx::*` presets, `mge::Song` | `AudioModule::loadWav("sounds/click.wav")` |
| **Shaders** | Embedded SPIR-V bytecode (`mge_add_shaders`) | None (shaders always compile at build time) |

### Path Resolution with `mge::assetPath()`

Always resolve asset paths using `mge::assetPath(...)`:
```cpp
#include <mge/core/Assets.h>

// Automatically checks MGE_ASSET_DIR env, adjacent assets, and parent folders
std::string path = mge::assetPath("textures/terrain.png");
```

---

## 13. Step-by-Step Tutorial: Building a Code-Only Game

In this section, we walk through building **Crystal Collector** (`examples/guide_game`), a complete 3D arcade game created entirely in C++ without loading a single external file.

### Step 1: CMake Target (`examples/guide_game/CMakeLists.txt`)

Define the game executable using `mge_add_game`:
```cmake
mge_add_game(guide_game
    SOURCES GuideGame.cpp
    MODULES platform gpu graphics audio
    SMOKE_FRAMES 60)
```

Add the subdirectory to `examples/CMakeLists.txt`:
```cmake
add_subdirectory(guide_game)
```

### Step 2: Game State & Setup

Create `examples/guide_game/GuideGame.cpp` with entities for collectible gems and hazards:
```cpp
#include <mge/mge.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

    struct Gem {
        glm::vec3 position{0.0f};
        float baseHeight = 0.8f;
        float bobPhase = 0.0f;
    };

    struct Hazard {
        glm::vec3 position{0.0f};
        float radius = 0.8f;
    };

    class GuideGame : public mge::Application {
    public:
        GuideGame() : mge::Application(makeConfig()) {
            addModule<mge::PlatformModule>();
            addModule<mge::GpuModule>();
            addModule<mge::AudioModule>();
        }
    ...
```

### Step 3: Input & Mesh Initialization in `onInit()`

Configure controls, renderers, procedural 3D meshes, and lighting:
```cpp
    bool onInit() override {
        m_platform = getModule<mge::PlatformModule>();
        m_gpu = getModule<mge::GpuModule>();
        m_audio = getModule<mge::AudioModule>();
        if (!m_platform || !m_gpu || !m_audio) return false;

        // 1. Actions
        auto& actions = m_platform->actions();
        actions.bindAxis("MoveX", mge::Key::D, mge::Key::A);
        actions.bindAxis("MoveZ", mge::Key::S, mge::Key::W);
        actions.bind("Jump", mge::Key::Space);
        actions.bind("Restart", mge::Key::R);
        actions.bind("Quit", mge::Key::Escape);

        // 2. Renderers
        mge::GpuDevice& device = m_gpu->device();
        m_renderer = std::make_unique<mge::MeshRenderer>(device, m_gpu->swapchainFormat());
        m_ui = std::make_unique<mge::Renderer2D>(device, m_gpu->swapchainFormat());

        // 3. Procedural meshes (zero asset files)
        m_floorMesh = device.uploadMesh(
            mge::primitives::plane(36.0f, 36.0f, 6, 6, glm::vec4(0.22f, 0.28f, 0.35f, 1.0f)));
        m_wallMesh = device.uploadMesh(
            mge::primitives::box(glm::vec3(1.0f, 2.5f, 1.0f), glm::vec4(0.38f, 0.44f, 0.52f, 1.0f)));
        m_playerMesh = device.uploadMesh(
            mge::primitives::uvSphere(0.6f, 24, 16, glm::vec4(0.25f, 0.85f, 1.0f, 1.0f)));
        m_gemMesh = device.uploadMesh(
            mge::primitives::icosphere(0.42f, 1, glm::vec4(1.0f, 0.85f, 0.20f, 1.0f)));
        m_hazardMesh = device.uploadMesh(
            mge::primitives::cone(0.65f, 1.2f, 16, glm::vec4(0.95f, 0.25f, 0.25f, 1.0f)));

        // 4. Lighting
        m_lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        m_lighting.sunColor = glm::vec3(1.0f, 0.95f, 0.85f);
        m_lighting.skyAmbient = glm::vec3(0.40f, 0.45f, 0.55f);
        m_lighting.groundAmbient = glm::vec3(0.20f, 0.18f, 0.16f);
        m_lighting.fog = true;
        m_lighting.fogColor = glm::vec3(0.10f, 0.12f, 0.16f);
        m_lighting.fogStart = 20.0f;
        m_lighting.fogEnd = 50.0f;

        // 5. Synthesized Audio
        m_coinSfx = m_audio->addSfx(mge::sfx::coin());
        m_jumpSfx = m_audio->addSfx(mge::sfx::jump());
        m_hurtSfx = m_audio->addSfx(mge::sfx::hurt());

        reset();
        return true;
    }
```

### Step 4: Fixed Physics Simulation in `onFixedUpdate()`

Process physics at 60 Hz with acceleration, gravity, jumping, boundary containment, and collision detection:
```cpp
    void onFixedUpdate(float fixedDt) override {
        if (m_gameOver) return;

        const float moveX = m_platform->axis("MoveX");
        const float moveZ = m_platform->axis("MoveZ");

        // Acceleration and damping
        const glm::vec3 targetVel(moveX * 12.0f, 0.0f, moveZ * 12.0f);
        m_playerVel.x = glm::mix(m_playerVel.x, targetVel.x, std::clamp(10.0f * fixedDt, 0.0f, 1.0f));
        m_playerVel.z = glm::mix(m_playerVel.z, targetVel.z, std::clamp(10.0f * fixedDt, 0.0f, 1.0f));

        // Gravity and jumping
        m_playerVel.y -= 26.0f * fixedDt;
        if (m_playerPos.y <= 0.61f && m_platform->isActionDown("Jump")) {
            m_playerVel.y = 10.5f;
            m_audio->play(m_jumpSfx);
        }

        m_playerPos += m_playerVel * fixedDt;
        if (m_playerPos.y < 0.6f) {
            m_playerPos.y = 0.6f;
            m_playerVel.y = 0.0f;
        }

        // Arena boundary containment
        constexpr float kBound = 16.5f;
        m_playerPos.x = glm::clamp(m_playerPos.x, -kBound, kBound);
        m_playerPos.z = glm::clamp(m_playerPos.z, -kBound, kBound);

        // Gem collection
        for (auto& gem : m_gems) {
            if (glm::distance(m_playerPos, gem.position) < 1.1f) {
                m_score += 100 * m_combo;
                m_combo++;
                m_audio->play(m_coinSfx);
                gem.position.x = randomRange(-14.0f, 14.0f);
                gem.position.z = randomRange(-14.0f, 14.0f);
            }
        }
    }
```

### Step 5: 3D Scene & 2D HUD Rendering in `onRender()`

Render 3D geometry with camera and lighting, followed by the immediate 2D UI overlay:
```cpp
    void onRender() override {
        const mge::GpuFrame* frame = m_gpu->frame();
        if (!frame || !m_renderer) return;

        // 3D Scene
        const float aspect = float(frame->width) / float(frame->height > 0 ? frame->height : 1);
        const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(55.0f), aspect, 0.1f, 100.0f);
        const glm::vec3 cameraPos = m_playerPos + glm::vec3(0.0f, 10.0f, 13.0f);
        const glm::mat4 view = glm::lookAt(cameraPos, m_playerPos + glm::vec3(0, 0.5f, 0), glm::vec3(0, 1, 0));

        if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height, {0.10f, 0.12f, 0.16f, 1.0f})) {
            m_renderer->setCamera(proj * view, cameraPos);
            m_renderer->setLighting(m_lighting);

            m_renderer->draw(m_floorMesh, glm::mat4(1.0f));

            for (const auto& gem : m_gems) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), gem.position);
                model = glm::rotate(model, gem.bobPhase * 1.5f, glm::vec3(0, 1, 0));
                m_renderer->draw(m_gemMesh, model);
            }

            m_renderer->draw(m_playerMesh, glm::translate(glm::mat4(1.0f), m_playerPos));
            m_renderer->end();
        }

        // 2D Immediate HUD
        m_ui->begin(frame->width, frame->height, 1.0f);
        m_ui->text({20.0f, 20.0f}, "SCORE: " + std::to_string(m_score), glm::vec4(1.0f), 2.0f);
        m_ui->flush(*frame->cmd, frame->swapchain);
    }
```

### Step 6: Shutdown Cleanup in `onShutdown()`

Release GPU buffers and renderers before the GPU device is terminated:
```cpp
    void onShutdown() override {
        m_floorMesh = {};
        m_wallMesh = {};
        m_playerMesh = {};
        m_gemMesh = {};
        m_hazardMesh = {};
        m_renderer.reset();
        m_ui.reset();
    }
```

### Step 7: Build and Run

Compile and launch the game:
```bash
cmake --preset dev
cmake --build --preset dev --target run-guide_game
```

The game compiles into a single binary, runs from any working directory, and requires no external asset files.
