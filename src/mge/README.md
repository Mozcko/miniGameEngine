# src/mge/

The engine. Each subfolder is one CMake target with its own `CMakeLists.txt` and `README.md`.

`#include <mge/mge.h>` (`src/mge/mge.h`) pulls in the common API: the whole core always, plus
platform, render and audio headers only if the program links that module. Each module target
exports `MGE_HAS_PROCGEN`, `MGE_HAS_VOXEL`, `MGE_HAS_PLATFORM`, `MGE_HAS_GPU`, `MGE_HAS_GRAPHICS` or `MGE_HAS_AUDIO`, so a headless program never needs
SDL headers. New to the engine? See [docs/FIRST_GAME.md](../../docs/FIRST_GAME.md).

| Folder | Target | Depends on | Contents | Doc |
|--------|--------|------------|----------|-----|
| `core/` | `mge_core` | std only | `Application` runner + `Module`, `Clock`/`Time`, `Input`, `Log`, ECS, cameras (`Camera` component, `FlyController`) | [core/README.md](core/README.md) |
| `procgen/` | `mge_procgen` | core (headless) | `MeshData`, OBJ export, primitives, mesh ops, noise, SDF + surface nets | [procgen/README.md](procgen/README.md) |
| `voxel/` | `mge_voxel` | core, procgen (headless) | `BlockRegistry`, chunks + coordinates, `VoxelWorld`, `TerrainGenerator`, `buildChunkMesh` | [voxel/README.md](voxel/README.md) |
| `platform/` | `mge_platform` | core, SDL3 | `PlatformModule` (window, SDL→core input), `Window`, `SdlInput` | [platform/README.md](platform/README.md) |
| `gpu/` | `mge_gpu` | core, procgen, platform, SDL3 | `GpuModule` (device + swapchain), move‑only GPU handles, `CommandBuffer`/passes, `uploadMesh` (SDL3 GPU, Vulkan) | [gpu/README.md](gpu/README.md) |
| `graphics/` | `mge_graphics` | gpu, procgen | `Texture2D` (file or pixels), `MeshRenderer` (texture × vertex colour, depth, opaque/transparent), embedded shaders | [graphics/README.md](graphics/README.md) |
| `audio/` | `mge_audio` | core, SDL3 | `AudioModule`, `AudioManager` (WAV SFX) | [audio/README.md](audio/README.md) |
| `app/` | `mge_main` | core | `Main.cpp` (`main()`) | [app/README.md](app/README.md) |


`-DMGE_BUILD_MODULES=OFF` builds only the headless modules (`mge_core`, `mge_procgen`, `mge_voxel`) and their tests, with no SDL or GPU needed.

## Writing a game on the engine (current API)

The quickest start is `scripts/new-game.sh <name>`; see [docs/FIRST_GAME.md](../../docs/FIRST_GAME.md).
The shape of a game:

```cpp
#include <mge/mge.h>

class MyGame : public mge::Application {
public:
    MyGame() : mge::Application(makeConfig()) {
        addModule<mge::PlatformModule>();   // window + input
        addModule<mge::GpuModule>();        // SDL3 GPU (after the platform)
    }
    void onUpdate(float dt) override { /* logic */ }
    void onRender() override {
        const mge::GpuFrame* frame = getModule<mge::GpuModule>()->frame();
        if (!frame) return;
        // MeshRenderer::begin(*frame->cmd, frame->swapchain, …) / draw / end
    }
};

mge::Application* mge::createApplication() { return new MyGame(); }
```

```cmake
mge_add_game(mygame SOURCES MyGame.cpp)   # MODULES default: platform gpu graphics audio
```
