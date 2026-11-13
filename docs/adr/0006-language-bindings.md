# 0006. Language Bindings Strategy: C API as Stable ABI, Python via nanobind

- Status: Accepted
- Date: 2026-11-11
- Issues: #160, #161, #162, #163, #164

## Context

miniGameEngine has evolved as a C++17 game engine where the engine owns the application entry point (`mge::main` / `mge_main`), and games interact directly with C++ classes, templated systems (such as `mge::Registry::add<T>()` and `each<A, B>()`), and RAII-managed GPU and audio resources.

Milestone M10 introduces support for writing games in **C** and **Python**, with the architectural goal of allowing future language bindings (such as Rust, Zig, Odin, C#, or Lua) without rewriting or compromising the engine core.

Binding multiple programming languages to a C++ engine exposes several fundamental challenges:

1. **C++ ABI Instability**: C++ lacks a standardized Application Binary Interface (ABI). Name mangling, memory layouts, virtual table structures, exceptions, standard library implementations (`libstdc++` vs. `libc++`), and template instantiations vary across compilers, operating systems, and optimization levels. Directly exposing C++ interfaces to foreign languages creates fragile, brittle integrations.
2. **`main()` Ownership and Control Inversion**: In C++ games, `mge_main` owns `main()`, runs initialization, creates the `mge::Application`, and drives the loop. In plain C, developers expect to write their own `main()` function. In Python, the interpreter owns the process entry point and executes a script top-to-bottom.
3. **ECS and Static Typing vs. Dynamic Runtimes**: The engine ECS (`mge::Registry`) relies on compile-time C++ types (`typeId<T>()` and `Pool<T>`). Foreign languages cannot instantiate C++ template classes at runtime. C programs and Python scripts require a mechanism to define, register, and query custom components dynamically at runtime.
4. **Threading and the Python Global Interpreter Lock (GIL)**: The engine's core and rendering pipelines run strictly on the main thread, while chunk streaming and audio mixing use background threads. Python executes with a GIL. Calling into Python from worker threads or keeping the GIL held during engine execution can introduce lock contention or fatal deadlocks during engine teardown.
5. **GPU Handle Lifetime and Shutdown Hang Trap**: In modern explicit graphics backends like SDL3 GPU (Vulkan, Metal, Direct3D 12), destroying or freeing GPU resources (buffers, textures, pipelines) after the `SDL_GPUDevice` has been destroyed leads to memory access violations, driver crashes, or hard hangs during process exit. In garbage-collected languages (like Python) or C programs with deferred cleanup, objects may be finalized after the engine application has already shut down.
6. **API and ABI Evolution**: As the engine evolves, binary compatibility must be maintained for compiled non-C++ games and dynamic language bindings without breaking older compiled binaries on minor version increments.

## Decision

We establish a two-tier language bindings architecture:

```
+-----------------------------------------------------------+
|               Game Code (C, Python, etc.)                 |
+-----------------------------+-----------------------------+
|    Plain C / Rust / Zig     |           Python            |
|       (Direct C ABI)        |     (import mge module)     |
+-----------------------------+-----------------------------+
|         C API v1            |       nanobind Layer        |
|    (mge_c / <mge/c/...>)    |    (Direct C++ bindings)    |
+-----------------------------+-----------------------------+
|                    miniGameEngine C++ Core                |
|    (mge_core, platform, gpu, graphics, audio, voxel, ...) |
+-----------------------------------------------------------+
```

### 1. C API (`mge_c`) as the Stable ABI Foundation

- **Standard and Boundary**: We implement a dedicated library, `mge_c`, exposing a pure C99-compatible interface declared in `<mge/c/mge_c.h>`.
- **Linkage and Calling Convention**: All exported functions use `extern "C"` linkage and standard platform calling conventions (`MGE_C_API`).
- **Opaque Handles**: All engine objects and modules are exposed as pointers to opaque structs:
  ```c
  typedef struct mge_app mge_app;
  typedef struct mge_mesh mge_mesh;
  typedef struct mge_texture mge_texture;
  typedef struct mge_voxel_world mge_voxel_world;
  typedef struct mge_block_registry mge_block_registry;
  ```
- **Data Types**: All public data types are Plain Old Data (POD) structs. No C++ classes, standard library containers (`std::vector`, `std::string`), templates, or C++ references cross the ABI boundary.
- **Error Handling**: Functions return an `mge_result` enum (`MGE_SUCCESS = 0`, `MGE_ERROR_* < 0`). Detailed error strings are retrieved via a thread-local function `const char* mge_last_error(void)`. No C++ exceptions cross the boundary; internal exceptions are caught at the boundary and converted to error codes.
- **Naming Conventions**: All C symbols use snake_case with a strict `mge_` prefix (e.g., `mge_app_create`, `mge_mesh_draw`, `mge_voxel_world_set_block`). Constants and macros use `MGE_` uppercase.

### 2. Application Entry Point and `main()` Ownership

- **C++ Games**: Continue to link `mge::main` (`mge_main`), which defines `main()` and invokes the user's `createApplication()`.
- **C Games**: The C game defines its own standard `int main(int argc, char** argv)` function. It configures the engine via `mge_app_config`, sets up function pointer callbacks (`on_init`, `on_fixed_update`, `on_update`, `on_render`, `on_shutdown`), and calls `mge_app_run(&config, &callbacks)`.
- **Python Games**: The Python script owns the top-level script execution, creates an `mge.Application(config)`, registers Python callbacks or subclass methods, and invokes `app.run()`.
- **Build Helper**: A CMake helper macro, `mge_add_c_game(name SOURCES ... [LIBRARIES ...])`, is provided to build pure C executables linked against `mge::c` without linking `mge::main`.

### 3. ECS Across Languages

- **Runtime Component Registration**: Foreign languages cannot access C++ template specializations. Instead, non-C++ code registers dynamic runtime components through the engine ECS:
  ```c
  mge_component_id mge_component_register(mge_app* app, const char* name, size_t size, size_t align);
  ```
- **Type-Erased Component Pools**: `mge::Registry` is extended with a type-erased `DynamicPool` alongside the existing templated `Pool<T>`. Dynamic components support `add`, `get`, `has`, `remove`, and iteration callbacks (`mge_component_each`).
- **Built-in Components**: In v1, arbitrary C++ user components are **not** visible to C or Python. Only core engine built-in components essential for game logic are explicitly exposed with dedicated C structs and accessors:
  - `mge_transform` (`Transform`: position, rotation, scale)
  - `mge_character_controller` (`CharacterController`: walk/fly modes, speed, jump, ground state)
  - `mge_rigid_body` (`RigidBody`: velocity, mass, gravity scale, restitution)
  - `mge_camera` (`Camera`: projection, FOV, near/far planes)

### 4. Python Bindings Architecture via nanobind

- **Direct C++ Binding via nanobind**: Python bindings (`import mge`) are implemented using **nanobind** directly over the C++ API, rather than wrapping the C API.
  - *Rationale*: nanobind generates exceptionally lean binaries with near-instant compilation times, zero-overhead type conversions, native exception translation, and automatic type annotations (`.pyi` stubs). Binding directly to C++ eliminates double-indirection overhead and boilerplate struct marshaling.
- **Scope Parity**: Python bindings expose the exact same functional surface area as the C API v1.
- **Packaging and Distribution**:
  - Built and packaged using `scikit-build-core` (`pyproject.toml` and CMake).
  - In v1, **no precompiled binary wheels are published to PyPI**. Developers build from source using standard pip:
    ```bash
    pip install .
    pip install git+https://github.com/Mozcko/miniGameEngine.git@v0.10.0
    ```

### 5. Threading and GIL Management

- **Main-Thread Engine Model**: miniGameEngine operates strictly on the main thread for game logic, windowing, and GPU rendering commands, mirroring the C++ engine architecture.
- **GIL Release and Re-acquisition**:
  - When `app.run()` executes the engine loop, it **releases the Python GIL** (`nb::gil_scoped_release`).
  - The engine loop runs at full native speed without GIL contention.
  - Whenever the engine invokes a user-defined Python callback (`on_init`, `on_fixed_update`, `on_update`, `on_render`, `on_shutdown`), the bindings **re-acquire the GIL** (`nb::gil_scoped_acquire`) for the duration of that callback.
  - *Deadlock Prevention*: Releasing the GIL during engine execution ensures that worker threads or shutdown operations waiting on task queues never deadlock with the Python interpreter holding the GIL.
- **Worker Threads and Callbacks**:
  - Python callbacks are **never** invoked from engine worker threads (such as the job system or audio thread).
  - When a custom Python terrain generator callback is registered for `VoxelWorld`, the chunk streamer is forced into **synchronous mode** on the main thread. This documented cost is necessary to prevent multi-threaded GIL contention, while keeping the binding simple and completely stable.

### 6. GPU Handle Lifetime and the Shutdown Trap

- **The Problem**: In the SDL3 GPU API, destroying a GPU resource (buffer, texture, pipeline) after `SDL_DestroyGPUDevice` causes memory corruption or process hangs. In C or Python, objects can outlive the application (e.g., global variables or delayed garbage collection during interpreter shutdown).
- **Tracked Handle Registry**:
  - Every GPU-backed handle created through C or Python (`mge_mesh*`, `mge_texture*`, etc.) is tracked in an internal registry owned by `mge_app`.
  - During application shutdown (`onShutdown()`), **before** the engine's `GpuModule` destroys the `SDL_GPUDevice`, the engine iterates through all active registered handles and safely releases their underlying GPU resources.
  - In Debug builds, the engine logs a warning for any leaked handle:
    ```
    [WARN] [C] Leaked 3 mesh handles at shutdown; auto-releasing
    ```
  - Released handles are marked as **inert** (`m_alive = false`). Subsequent calls using an inert handle cleanly return `MGE_ERROR_INVALID_HANDLE` in C or raise a `RuntimeError` in Python, rather than dereferencing dangling pointers or invoking deleted GPU devices.
  - When foreign code later calls `mge_mesh_free(handle)` or the Python garbage collector finalizes the object, the destructor safely no-ops on the inert handle.

### 7. Memory Ownership Conventions

- **Resource Lifecycles**:
  - Resources allocated by the user are freed via explicit destroy functions (e.g., `mge_mesh_destroy`, `mge_texture_destroy`, `mge_voxel_world_destroy`).
  - Resources not explicitly freed before shutdown are automatically reclaimed during application teardown.
- **Strings**:
  - Strings returned by the C API (e.g., `mge_last_error()`) return borrowed `const char*` pointers valid until the next C API call on the same thread, or static string literals. The caller must copy the string if long-term persistence is needed.
  - Strings passed into the C API as parameters are copied internally by the engine.
- **Buffers and Arrays**:
  - Data buffers passed into the C API (e.g., vertex arrays, index arrays, voxel data, raw audio samples) are copied by the engine during the call. The caller retains ownership of the input buffer and may free or reuse it immediately after the function returns.

### 8. Versioning and ABI Compatibility Rules

- **Semantic Versioning**:
  - The C API header exposes version macros:
    ```c
    #define MGE_C_API_VERSION_MAJOR 1
    #define MGE_C_API_VERSION_MINOR 0
    ```
  - A runtime function `uint32_t mge_c_api_version(void)` returns the compiled API version `(MAJOR << 16) | MINOR`.
  - The C API version is **independent** of the engine package version (e.g., engine package `0.10.0` exposes C API `1.0`).
- **Struct Evolution via Size Field**:
  - All configuration and descriptor structs passed by pointer carry `size_t struct_size` as their first field:
    ```c
    typedef struct mge_app_config {
        size_t struct_size;
        const char* title;
        int32_t width;
        int32_t height;
        bool vsync;
        /* Future fields appended here */
    } mge_app_config;
    ```
  - The engine verifies `struct_size` to maintain backward binary compatibility with callers compiled against older struct definitions. Structs are strictly **append-only**.
- **Enums**: Enum values are assigned explicit, stable 32-bit integer values and are never renumbered or reordered.
- **Deprecation Policy**: Deprecated functions or fields are marked with `MGE_DEPRECATED` and preserved for at least one minor version before removal.

### 9. Feature Scope for Milestone v1

| Subsystem | Included in v1 (In Scope) | Excluded in v1 (Out of Scope) |
|---|---|---|
| **App & Loop** | App lifecycle, fixed timestep, alpha interpolation, delta time, exit codes | Multi-window support, headless server mode via C |
| **Input** | Key queries, mouse position/buttons, relative mouse mode, action mapping | Gamepad/joystick mappings, custom touch input |
| **ECS** | Runtime component registration, entity lifecycle, component CRUD, iteration, phase systems | Dynamic C++ template reflection, foreign script queries on arbitrary C++ components |
| **Built-in Components** | `Transform`, `CharacterController`, `RigidBody`, `Camera` | Specialized internal C++ components |
| **Procgen & Meshes** | Procedural primitives (cube, sphere, cylinder, plane), custom vertex/index mesh upload, `MeshRenderer` draw calls with transform and tint | Custom vertex layouts, manual GPU buffer sub-allocation |
| **Environment & Sky** | Directional sunlight, ambient sky/ground colors, fog parameters, sky/day-night time of day | Custom procedural skybox shaders |
| **2D UI & Text** | Immediate-mode rectangles, outlines, lines, embedded 8x8 font text, texture blitting | SDF vector font rendering, rich UI widget hierarchy |
| **Audio** | Procedural SFX presets/params, WAV playback, sound instances, background music playback, bus volumes | Custom runtime DSP synthesis nodes, raw audio streaming callbacks |
| **Voxel World** | Custom block registry, voxel world creation/manipulation, DDA raycasting, terrain generation callback (synchronous streaming) | Multi-threaded Python generator execution, runtime chunk LOD configuration |
| **Physics** | Swept AABB voxel collision, character controller movement, rigid bodies | Custom collision shapes beyond AABB/voxels |
| **Shaders & GPU** | Default PBR/unlit pipeline usage | User-authored custom GLSL/SPIR-V shaders, GPU compute passes, custom render passes, render queue sorting |
| **Profiler** | Read-only aggregate frame statistics (FPS, frame time, draw calls) | Detailed CPU/GPU micro-profiler timeline instrumentation |

## Alternatives Considered

### 1. Python over C API via `ctypes` or `cffi`
- **Description**: Expose the C API and write a Python wrapper using standard library `ctypes` or `cffi` without compiling a C++ Python extension.
- **Why Not**:
  - *Performance*: Significant overhead on per-frame math transformations, vector operations, and entity iteration.
  - *Ergonomics*: Requires extensive manual boilerplate, duplicate struct declarations in Python, and error-prone pointer arithmetic.
  - *Type Safety and Tooling*: Difficult to provide first-class IDE autocompletion, type hinting (`.pyi`), and docstrings without a redundant secondary wrapper layer.

### 2. SWIG (Simplified Wrapper and Interface Generator)
- **Description**: Generate bindings for C, Python, and other languages from interface definition files.
- **Why Not**:
  - Generates convoluted, fragile C wrapper code that is difficult to debug and customize.
  - Weak support for modern C++17 idioms.
  - Substantial binary bloat and external build-time dependency conflicting with the engine's minimalist philosophy.

### 3. pybind11
- **Description**: Use pybind11 to create Python extensions directly from C++.
- **Why Not**:
  - pybind11 is mature and capable, but causes notable binary size growth (1.5–2.5 MB minimum for small modules) and slow C++ compilation times due to heavy template metaprogramming.
  - **nanobind** was developed by the same author specifically as the modern successor to pybind11, providing up to 4× smaller binary footprints, 3× faster compilation times, and seamless support for C++17 and Python 3.8+.

### 4. Direct Python Bindings Over the C API
- **Description**: Write the nanobind module against `<mge/c/mge_c.h>` instead of C++ classes.
- **Why Not**:
  - Introduces an unnecessary layer of marshaling: C++ -> C API -> Python binding -> Python.
  - Nanobind natively binds C++ types (e.g., `glm::vec3`, `std::string_view`, `std::optional`, C++ exceptions) with zero overhead and expressive syntax.
  - Keeping both the C API and nanobind as sibling consumers of the C++ core ensures optimal developer ergonomics and performance for both languages.

## Consequences

- **Universal Accessibility**: Any language supporting a standard C foreign function interface (FFI)—including Rust, Zig, C#, Go, and Lua—can target miniGameEngine via `mge_c` with guaranteed ABI stability.
- **First-Class Python Experience**: Python developers receive idiomatic, performant bindings with compact binary sizes and complete type annotations.
- **Crash-Proof Shutdown**: The tracked handle architecture completely eliminates device teardown crashes and memory corruptions caused by language garbage collection or out-of-order freeing.
- **Predictable Evolution**: Strict struct size checking and append-only rules protect binary compatibility across future minor releases.
- **Clear Roadmap Execution**: This ADR directly enables the implementation of Milestone M10:
  - Issue #161: C API implementation (`mge_c`, `<mge/c/mge_c.h>`).
  - Issue #162: Python bindings via nanobind (`import mge`).
  - Issue #163: CLI templates and tooling (`mge new --lang c|python`).
