# C API (`mge_c`) Reference & Guide

The `mge_c` module exposes a pure C99-compatible Application Binary Interface (ABI) for `miniGameEngine`. It serves as the foundation for creating games in plain C and acts as the low-level interoperability layer for other foreign language bindings (e.g. Zig, Rust, C#, Go).

---

## 1. Design & Core Rules

### C99 ABI & Opaque Handles
- Defined entirely in the single public header `<mge/c/mge_c.h>`.
- Only includes `<stddef.h>`, `<stdint.h>`, and `<stdbool.h>`. No C++ standard library types or templates are exposed.
- All engine handles are opaque pointer types:
  - `mge_app*`: Engine application and subsystem coordinator.
  - `mge_mesh*`: Uploaded procedural or custom 3D mesh.
  - `mge_texture*`: 2D RGBA texture resource.
  - `mge_audio_sample*`: Sound effect or audio sample loaded into memory.
  - `mge_block_registry*`: Voxel block catalog.
  - `mge_voxel_world*`: Voxel grid and world manager.
- Entities are 64-bit scalar identifiers (`mge_entity`). The invalid entity is `MGE_NULL_ENTITY`.

### `main()` Ownership
In C++, games hook into the engine via `mge_main`. In C, your code owns `main()` and drives the engine lifecycle explicitly:
```c
int main(int argc, char* argv[]) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.title = "My C Game";
    config.width = 1280;
    config.height = 720;

    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init;
    callbacks.on_update = on_update;
    callbacks.on_render = on_render;

    return (int)mge_app_run(&config, &callbacks);
}
```

### Memory Ownership Rules
1. **Explicit Destruction**: Any handle created by an `mge_*_create*()` function must be freed with its corresponding `mge_*_destroy(handle)` function (e.g. `mge_mesh_destroy`, `mge_texture_destroy`, `mge_audio_sample_destroy`, `mge_block_registry_destroy`, `mge_voxel_world_destroy`).
2. **Borrowed Strings**: Strings returned by the engine (such as `mge_last_error()` and `mge_version_string()`) are borrowed pointers with thread-local lifetime. They remain valid until the next C API call on the same thread. Do not call `free()` on them.
3. **Copied Buffers**: Input arrays and strings passed to the engine (e.g., vertex arrays, pixel buffers, action names) are copied or processed synchronously by the call. The caller retains ownership and may safely free or reuse them immediately after the function returns.

### Threading Rules
- All C API functions must be called exclusively from the main application thread, mirroring the C++ engine architecture.
- Callbacks (`on_init`, `on_update`, `on_fixed_update`, `on_render`, `on_shutdown`, ECS system callbacks, and each queries) execute synchronously on the main thread.

### GPU Handle Lifetime & Shutdown Safety
Every GPU resource (`mge_mesh`, `mge_texture`) is registered with the engine's `GpuResourceTracker`. When `mge_app` shuts down, all live GPU handles are released *before* the underlying `GpuDevice` is destroyed. If an application exits without destroying a mesh or texture, a warning is logged in Debug builds, but no crash or GPU hang occurs. Destroying a handle after engine shutdown is guaranteed to be a safe no-op.

### Versioning & ABI Extensibility
- **Header Versions**: `MGE_C_API_VERSION_MAJOR` (1) and `MGE_C_API_VERSION_MINOR` (0).
- **Runtime Query**: `mge_c_api_version()` returns `(major << 16) | minor`.
- **Struct Evolution**: All configuration structs passed by pointer carry `size_t struct_size` as their first field. Structs are strictly append-only; existing field offsets and types never change across minor versions.
- **Enums**: Enum values are never renumbered. Deprecated values remain available for at least one minor release cycle before removal.

---

## 2. Minimal Example

Below is a complete standalone C program creating a procedural spinning cube with procedural audio and 2D UI text:

```c
#include <mge/c/mge_c.h>

#include <math.h>
#include <stdbool.h>
#include <string.h>

typedef struct GameState {
    mge_mesh* cube;
    mge_audio_sample* coinSound;
    float rotationY;
} GameState;

static bool on_init(mge_app* app, void* user_data) {
    GameState* state = (GameState*)user_data;

    // Create procedural unit cube
    state->cube = mge_mesh_create_cube(app, 1.0f);
    if (!state->cube) return false;

    // Create procedural retro sound effect
    state->coinSound = mge_audio_sfx_create_preset(app, MGE_SFX_COIN, 0);

    // Bind input actions
    mge_input_bind_action(app, "Quit", "Escape");
    mge_input_bind_action(app, "Coin", "Space");

    // Setup camera view & perspective projection
    float view[16] = {
        1.0f,  0.0f,          0.0f,          0.0f,
        0.0f,  0.89442719f,  -0.44721360f,   0.0f,
        0.0f,  0.44721360f,   0.89442719f,   0.0f,
        0.0f, -0.89442719f,  -4.47213595f,   1.0f
    };
    const float f = 1.0f / tanf(1.04719755f * 0.5f); // 60 deg FOV
    float proj[16] = {0};
    proj[0] = f / (800.0f / 600.0f);
    proj[5] = f;
    proj[10] = 100.0f / (0.1f - 100.0f);
    proj[11] = -1.0f;
    proj[14] = (0.1f * 100.0f) / (0.1f - 100.0f);
    mge_renderer_set_camera(app, view, proj);

    return true;
}

static void on_update(mge_app* app, float dt, void* user_data) {
    GameState* state = (GameState*)user_data;

    if (mge_input_is_action_pressed(app, "Quit")) {
        mge_app_request_stop(app);
    }
    if (mge_input_is_action_pressed(app, "Coin") && state->coinSound) {
        mge_audio_play(app, state->coinSound, 1.0f, 0.0f);
    }

    state->rotationY += dt * 1.5f;
    mge_sky_advance_time(app, dt * 0.1f);
}

static void on_render(mge_app* app, void* user_data) {
    GameState* state = (GameState*)user_data;

    const float c = cosf(state->rotationY);
    const float s = sinf(state->rotationY);
    float model[16] = {
         c,    0.0f,  -s,   0.0f,
        0.0f,  1.0f, 0.0f,  0.0f,
         s,    0.0f,   c,   0.0f,
        0.0f,  0.0f, 0.0f,  1.0f
    };

    float color[4] = {0.2f, 0.7f, 1.0f, 1.0f};
    mge_renderer_draw_mesh(app, state->cube, model, color);

    float textColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    mge_ui_text(app, 20.0f, 20.0f, "Hello from C", 2.0f, textColor);
}

static void on_shutdown(mge_app* app, void* user_data) {
    (void)app;
    GameState* state = (GameState*)user_data;
    if (state->coinSound) mge_audio_sample_destroy(state->coinSound);
    if (state->cube) mge_mesh_destroy(state->cube);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.title = "Hello C";
    config.width = 800;
    config.height = 600;

    GameState state;
    memset(&state, 0, sizeof(state));

    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init;
    callbacks.on_update = on_update;
    callbacks.on_render = on_render;
    callbacks.on_shutdown = on_shutdown;
    callbacks.user_data = &state;

    return (int)mge_app_run(&config, &callbacks);
}
```

---

## 3. Building Games in C with CMake

Add your C game using the CMake helper `mge_add_c_game()` declared in `MgeGame.cmake`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_c_game LANGUAGES C CXX)

find_package(mge 1.0 REQUIRED)

mge_add_c_game(my_c_game
    SOURCES Main.c
    SMOKE_FRAMES 60
)
```

`mge_add_c_game` automatically:
- Enforces the C99 standard (`C_STANDARD 99`).
- Configures warning flags under `-Wall -Wextra -Werror` (`MGE_WERROR`).
- Sets `LINKER_LANGUAGE CXX` so the underlying C++ runtime and engine dependencies link correctly.
- Registers a smoke test target run under CTest (`smoke: my_c_game offscreen 60 frames`).
- Exposes a `run-my_c_game` build target for convenience.

---

## 4. API Reference Summary

### Core & Application
| Function | Description |
| :--- | :--- |
| `mge_c_api_version()` | Returns `(MGE_C_API_VERSION_MAJOR << 16) \| MGE_C_API_VERSION_MINOR`. |
| `mge_version_string()` | Returns the engine semantic version string (e.g. `"1.0.0"`). |
| `mge_last_error()` | Returns thread-local borrowed error message string. |
| `mge_app_run(config, callbacks)` | Starts and runs the application main loop. Returns `mge_result`. |
| `mge_app_request_stop(app)` | Requests application loop exit after the current frame. |
| `mge_time_delta(app)` | Returns scaled delta time for the current frame. |
| `mge_time_elapsed(app)` | Returns total elapsed simulation time. |
| `mge_time_frame_count(app)` | Returns total number of rendered frames. |
| `mge_time_set_scale(app, scale)` | Sets time scale multiplier (1.0 = normal, 0.0 = pause). |

### Input
| Function | Description |
| :--- | :--- |
| `mge_input_bind_action(app, action, key)` | Binds an action string to a physical key name (e.g. `"Space"`). |
| `mge_input_bind_axis(app, axis, neg, pos)` | Binds an axis string to two keys in `[-1.0, 1.0]`. |
| `mge_input_is_action_pressed(app, action)` | True if action transitioned down during the current frame. |
| `mge_input_is_action_down(app, action)` | True while the action key is currently held down. |
| `mge_input_is_action_released(app, action)` | True if action transitioned up during the current frame. |
| `mge_input_get_axis(app, axis)` | Returns current value of an axis in `[-1.0, 1.0]`. |
| `mge_input_mouse_delta(app, &dx, &dy)` | Reads mouse motion delta accumulated in the current frame. |
| `mge_input_set_cursor_lock(app, locked)` | Captures and hides the OS cursor for FPS-style controls. |
| `mge_test_inject_key(app, key, down)` | Injects synthetic key events for automated tests. |

### Entity Component System (ECS)
| Function | Description |
| :--- | :--- |
| `mge_entity_create(app)` | Creates a new active entity. |
| `mge_entity_destroy(app, entity)` | Destroys an entity and all its attached components. |
| `mge_entity_is_valid(app, entity)` | Checks if an entity exists and is alive. |
| `mge_transform_{set,get,has,remove}` | Built-in 3D transform component (position, quaternion, scale). |
| `mge_camera_{set,get,has,remove}` | Built-in camera component (FOV, planes, orthographic toggle). |
| `mge_character_controller_{set,get,has,remove}` | Built-in kinematic character controller. |
| `mge_rigid_body_{set,get,has,remove}` | Built-in dynamic / kinematic rigid body component. |
| `mge_component_register(app, name, size, align)` | Registers a custom component type in the runtime pool. |
| `mge_component_add(app, entity, id)` | Allocates and attaches a zero-initialized component. |
| `mge_component_get(app, entity, id)` | Returns pointer to raw component memory, or NULL. |
| `mge_component_has(app, entity, id)` | Checks if entity possesses the component type. |
| `mge_component_remove(app, entity, id)` | Detaches the component. |
| `mge_component_each(app, ids, count, cb, user_data)` | Iterates entities matching all component types simultaneously. |
| `mge_system_add(app, phase, name, fn, user_data)` | Schedules a system callback in `MGE_PHASE_FIXED/UPDATE/RENDER`. |

### Rendering & UI
| Function | Description |
| :--- | :--- |
| `mge_mesh_create_{cube,sphere,plane,cylinder}` | Generates procedural 3D primitives. |
| `mge_mesh_upload(app, vertices, v_count, indices, i_count)` | Uploads raw vertex positions and index buffers to GPU. |
| `mge_mesh_destroy(mesh)` | Frees GPU and CPU resources for a mesh. |
| `mge_renderer_draw_mesh(app, mesh, model, color)` | Draws a mesh within the active 3D render pass. |
| `mge_renderer_set_lighting(app, &lighting)` | Configures directional sun, ambient sky/ground, and fog. |
| `mge_renderer_set_camera(app, view, proj)` | Sets active view and projection matrices. |
| `mge_sky_set_time_of_day(app, hour)` | Sets sun angle and atmospheric sky gradient (0.0 to 24.0). |
| `mge_sky_advance_time(app, hours)` | Advances day/night cycle. |
| `mge_ui_rect(app, x, y, w, h, color)` | Draws a filled 2D rectangle in screen pixel space. |
| `mge_ui_rect_outline(app, x, y, w, h, color, thickness)` | Draws a stroked rectangle outline. |
| `mge_ui_line(app, x0, y0, x1, y1, color, thickness)` | Draws a 2D line segment. |
| `mge_ui_text(app, x, y, text, scale, color)` | Renders text using the embedded 8x8 bitmap font. |
| `mge_texture_create_rgba(app, w, h, pixels)` | Uploads 2D RGBA8 pixel buffer to GPU. |
| `mge_texture_destroy(texture)` | Frees texture GPU memory. |
| `mge_ui_image(app, texture, x, y, w, h, color)` | Renders textured 2D sprite. |

### Audio & Synthesis
| Function | Description |
| :--- | :--- |
| `mge_audio_sfx_create_preset(app, preset, seed)` | Synthesizes a retro sound (`JUMP`, `COIN`, `HURT`, `EXPLOSION`, `POWER_UP`, `CLICK`). |
| `mge_audio_sfx_create_custom(app, &params)` | Synthesizes custom sfxr sound parameters. |
| `mge_audio_sample_load_wav(app, path)` | Loads and converts a WAV file to mixer format. |
| `mge_audio_sample_destroy(sample)` | Frees sample audio memory. |
| `mge_audio_sample_frame_count(sample)` | Number of audio frames in the sample. |
| `mge_audio_sample_rms(sample)` | Computes root-mean-square amplitude of the sample. |
| `mge_audio_play(app, sample, gain, pan)` | Plays an audio sample through the effects bus. |
| `mge_audio_song_play(app, song_name, fade)` | Plays embedded theme music (`"platformer"`, `"night"`, `"ambient"`). |
| `mge_audio_song_stop(app, fade)` | Stops currently playing music. |
| `mge_audio_set_master_volume(app, volume)` | Sets master volume multiplier in `[0.0, 1.0]`. |
| `mge_audio_set_sfx_volume(app, volume)` | Sets sound effects bus volume. |
| `mge_audio_set_music_volume(app, volume)` | Sets music bus volume. |
| `mge_audio_set_mute(app, mute)` | Mutes or unmutes all audio playback. |

### Voxel World & Physics
| Function | Description |
| :--- | :--- |
| `mge_block_registry_create(app)` | Creates a new block definition catalog. |
| `mge_block_register(reg, &desc)` | Registers a block type (solid, opaque, color). Returns `block_id`. |
| `mge_block_registry_destroy(reg)` | Frees a block registry. |
| `mge_voxel_world_create(app, reg, gen_fn, user_data)` | Creates a chunked 3D voxel world with optional procedural callback. |
| `mge_voxel_world_destroy(world)` | Destroys the voxel world. |
| `mge_voxel_world_get_block(world, x, y, z)` | Gets block ID at world integer coordinates (auto-loads chunk if generator exists). |
| `mge_voxel_world_set_block(world, x, y, z, id)` | Sets block ID at world integer coordinates (auto-loads chunk). |
| `mge_voxel_world_raycast(world, origin, dir, max_dist, &hit)` | Casts ray through voxel world using fast DDA; returns hit coordinates and normal. |
