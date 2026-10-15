# src/mge/

The engine. Each subfolder is one CMake target with its own `CMakeLists.txt` and `README.md`.

`#include <mge/mge.h>` (`src/mge/mge.h`) pulls in the common API: the whole core always, plus
platform, render and audio headers only if the program links that module. Each module target
exports `MGE_HAS_PROCGEN`, `MGE_HAS_VOXEL`, `MGE_HAS_PLATFORM`, `MGE_HAS_GPU`, `MGE_HAS_GRAPHICS`, `MGE_HAS_RENDER` or `MGE_HAS_AUDIO`, so a headless program never needs
SDL headers. New to the engine? See [docs/FIRST_GAME.md](../../docs/FIRST_GAME.md).

| Folder | Target | Depends on | Contents | Doc |
|--------|--------|------------|----------|-----|
| `core/` | `mge_core` | std only | `Application` runner + `Module`, `Clock`/`Time`, `Input`, `Log` | [core/README.md](core/README.md) |
| `procgen/` | `mge_procgen` | core (headless) | `MeshData`, OBJ export, primitives, mesh ops, noise, SDF + surface nets | [procgen/README.md](procgen/README.md) |
| `voxel/` | `mge_voxel` | core, procgen (headless) | `BlockRegistry`, chunks + coordinates, `VoxelWorld`, `TerrainGenerator`, `buildChunkMesh` | [voxel/README.md](voxel/README.md) |
| `platform/` | `mge_platform` | core, SDL3, GLEW, OpenGL | `PlatformModule` (window, GL 3.3 context, GLEW, SDL→core input), `Window`, `SdlInput` | [platform/README.md](platform/README.md) |
| `gpu/` | `mge_gpu` | core, procgen, platform, SDL3 | `GpuModule` (device + swapchain), move‑only GPU handles, `CommandBuffer`/passes, `uploadMesh` (SDL3 GPU, Vulkan) | [gpu/README.md](gpu/README.md) |
| `graphics/` | `mge_graphics` | gpu, procgen | `Texture2D` (file or pixels), `MeshRenderer` (texture × vertex colour, depth, opaque/transparent), embedded shaders | [graphics/README.md](graphics/README.md) |
| `render/` | `mge_render` | core, GL, GLEW | `core/` Vertex, Texture, GLSLProgram, ResourceManager · `3d/` Mesh · `2d/` Sprite, Animation (legacy) · `camera/` Camera, Camera2D, Camera3D, FirstPersonCamera | [render/README.md](render/README.md) |
| `audio/` | `mge_audio` | core, SDL3 | `AudioModule`, `AudioManager` (WAV SFX) | [audio/README.md](audio/README.md) |
| `app/` | `mge_main` | core | `Main.cpp` (`main()`) | [app/README.md](app/README.md) |


`-DMGE_BUILD_MODULES=OFF` builds only the headless modules (`mge_core`, `mge_procgen`, `mge_voxel`) and their tests, with no SDL or GPU needed.

## How M1 reshapes this
- ~~**#11** moves `Application` into `mge_core` as a module‑based runner~~ (done).
- ~~**#9** moves platform and audio to SDL3, and removes SDL_mixer, SDL_ttf and `GUILabel`~~ (done).
- **#10** puts everything in one `mge` namespace with a single naming convention.

## Writing a game on the engine (current API)

```cpp
#include <mge/core/Application.h>
#include <mge/platform/PlatformModule.h>

class MyGame : public mge::Application {
public:
    MyGame() { addModule<mge::PlatformModule>(); }       // omit for a headless program
protected:
    bool onInit() override {
        getModule<mge::PlatformModule>()->setEventCallback([this](const SDL_Event& e) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) requestStop();
        });
        return true;                                       // false = abort startup (exit code 1)
    }
    void onUpdate(float dt) override { /* logic */ }
    void onRender() override { /* glClear + draw; PlatformModule swaps buffers */ }
};

mge::Application* mge::createApplication() { return new MyGame(); }
```

```cmake
add_executable(mygame MyGame.cpp)
target_link_libraries(mygame PRIVATE mge_platform mge_render mge_main)
```

The window starts with **depth test disabled** and blending enabled (see `Window::init`). Enable
depth testing yourself for 3D.
