# mge_c: C API for miniGameEngine

The `mge_c` module exposes a stable, C99-compliant interface to `miniGameEngine` for plain C games and foreign language bindings (ADR 0006).

For the complete API reference, architecture rules, and guides, see [docs/C_API.md](../../../docs/C_API.md).

## Source Structure

| File | Purpose |
| :--- | :--- |
| `mge_c.h` | Pure C99 public header defining the stable ABI, opaque handles, and function signatures. |
| `mge_c_internal.h` | Internal C++ translation definitions, bridging `mge_app` to `Application`, `World`, and engine modules. |
| `App.cpp` | Application lifecycle, configuration, time queries, and error handling (`mge_app_run`, `mge_last_error`). |
| `Errors.cpp` | Thread-local error string storage and retrieval. |
| `Input.cpp` | Action and axis bindings, key queries, mouse delta, and test key injection. |
| `Ecs.cpp` | Entity lifecycle, built-in components (Transform, Camera, CharacterController, RigidBody), runtime component registration, and systems. |
| `Render.cpp` | Procedural meshes, GPU mesh uploads, drawing, lighting, camera matrices, sky day/night, and 2D UI drawing. |
| `Audio.cpp` | Procedural sfxr sound synthesis, WAV loading, playback, music songs, buses, and volume controls. |
| `VoxelPhysics.cpp` | Block registration, voxel world management with C callbacks, and raycasting. |

## Quick Start

```cmake
# In your CMakeLists.txt
find_package(mge 0.9 REQUIRED)

mge_add_c_game(my_c_game
    SOURCES Main.c
    SMOKE_FRAMES 60
)
```

```c
// In Main.c
#include <mge/c/mge_c.h>

static bool on_init(mge_app* app, void* user_data) {
    mge_log_info("Game", "Running in pure C!");
    return true;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    mge_app_config config = {
        .struct_size = sizeof(mge_app_config),
        .title = "C Game",
        .width = 800,
        .height = 600
    };
    mge_app_callbacks callbacks = {
        .struct_size = sizeof(mge_app_callbacks),
        .on_init = on_init
    };
    return (int)mge_app_run(&config, &callbacks);
}
```
