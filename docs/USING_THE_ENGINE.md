# Using the Engine as a Dependency

`miniGameEngine` is consumable as a standalone reusable CMake dependency, either installed as a CMake package via `find_package(mge)` or fetched into your project via `FetchContent`.

---

## 1. Using `find_package(mge)`

### Installing the Engine

To install the engine as a system or local CMake package, configure the engine repository with `-DMGE_INSTALL=ON` and choose an install prefix:

```bash
# SDL3 must be installed on your system (e.g. SDL3-devel on Fedora, or built & installed to /usr/local)
cmake -S . -B build-install -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/mge \
    -DMGE_INSTALL=ON \
    -DMGE_BUILD_TESTS=OFF \
    -DMGE_BUILD_BENCHMARKS=OFF \
    -DMGE_BUILD_EXAMPLES=OFF \
    -DMGE_BUILD_SANDBOX=OFF

cmake --build build-install
cmake --install build-install
```

> [!NOTE]
> Installing `miniGameEngine` with a FetchContent-built SDL3 is explicitly not supported. `MGE_INSTALL=ON` requires SDL3 to be installed on your system.

### Consumer `CMakeLists.txt`

A minimal consumer `CMakeLists.txt` requires only a few lines:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_game LANGUAGES C CXX)

find_package(mge 0.9 REQUIRED)

mge_add_game(my_game SOURCES Main.cpp SMOKE_FRAMES 30)
```

To configure and build against your installed prefix:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/mge
cmake --build build
```

---

## 2. Using `FetchContent`

You can also incorporate `miniGameEngine` directly into your project without any prior installation step:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_game LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(
    mge
    GIT_REPOSITORY https://github.com/Mozcko/miniGameEngine.git
    GIT_TAG        v0.9.0
)
FetchContent_MakeAvailable(mge)

mge_add_game(my_game SOURCES Main.cpp SMOKE_FRAMES 30)
```

> [!NOTE]
> `GIT_TAG v0.9.0` will be available after milestone M9 is merged and tag `v0.9.0` is pushed. For local development or prior to the release tag, use `-DFETCHCONTENT_SOURCE_DIR_MGE=/path/to/miniGameEngine`.

When consumed via `FetchContent`, tests, benchmarks, examples, and the sandbox are disabled automatically (`MGE_BUILD_TESTS`, `MGE_BUILD_BENCHMARKS`, `MGE_BUILD_EXAMPLES`, and `MGE_BUILD_SANDBOX` default to `OFF`). Only the engine library targets are compiled.

---

## 3. Available Targets

Both in-tree builds and consumer projects (via `find_package` or `FetchContent`) use the exact same `mge::` target names:

| Target | Description | Headless? |
|---|---|---|
| `mge::core` | Headless core: Application lifecycle, ECS, math/geometry, clock, time, log, input | Yes |
| `mge::procgen` | Procedural generation: mesh data, primitives, noise, SDF | Yes |
| `mge::voxel` | Voxel chunks, block registry, world storage, mesher, raycasting | Yes |
| `mge::synth` | Audio synthesis: oscillators, envelopes, filters, delays, mixer | Yes |
| `mge::physics` | Physics: swept AABB voxel collision, spatial hash, rigid bodies, character controller | Yes |
| `mge::main` | Engine `main()` object library (invokes game `mge::createApplication()`) | Yes |
| `mge::platform` | Platform layer: SDL3 window and input mapping | No (SDL3) |
| `mge::gpu` | Low-level GPU layer: SDL3 GPU device, command buffers, pipelines | No (SDL3 GPU) |
| `mge::graphics` | High-level 3D/2D renderer: textures, mesh rendering, 2D batching | No (SDL3 GPU) |
| `mge::audio` | Audio output: connects `mge::synth` mixer to SDL3 audio streams | No (SDL3) |

---

## 4. Helper Functions

### `mge_add_game`

The `mge_add_game` function defines a game executable, sets up standard compilation warnings, links `mge::main` and any requested engine modules, outputs the binary to the consumer's build directory root, and optionally registers an offscreen smoke test:

```cmake
mge_add_game(<name>
    SOURCES <sources...>
    [LIBRARIES <extra_libs...>]
    [MODULES platform gpu graphics audio]  # Default: all 4 modules
    [HEADLESS]                             # No window/GPU/SDL modules
    [SMOKE_FRAMES <n>]                     # Adds offscreen/headless smoke test for CTest
)
```

### `mge_add_shaders`

Compiles GLSL shaders into embedded C++ SPIR-V headers at build time:

```cmake
mge_add_shaders(<target>
    SHADERS <shader.vert> <shader.frag> <shader.comp> ...
    [INCLUDE_DIRS <dirs...>]
)
```

The shader compiler (`glslang` or `glslangValidator`) must be available in your `PATH`. Shaders can `#include "mge/lighting.glsl"`, `#include "mge/raymarch.glsl"`, `#include "mge/sdf.glsl"`, or `#include "mge/sdf_pass.glsl"`.

---

## 5. CMake Options Reference

When configuring `miniGameEngine`:

| Option | Default | Description |
|---|---|---|
| `MGE_INSTALL` | `OFF` | Export and generate CMake installation rules (`find_package(mge)`). |
| `MGE_BUILD_MODULES` | `ON` | Build SDL3/GPU modules (`platform`, `gpu`, `graphics`, `audio`). `OFF` builds headless core only. |
| `MGE_BUILD_TESTS` | `${PROJECT_IS_TOP_LEVEL}` | Build unit test executables with doctest. |
| `MGE_BUILD_BENCHMARKS` | `${PROJECT_IS_TOP_LEVEL}` | Build headless microbenchmarks. |
| `MGE_BUILD_EXAMPLES` | `${PROJECT_IS_TOP_LEVEL}` | Build example projects under `examples/`. |
| `MGE_BUILD_SANDBOX` | `${PROJECT_IS_TOP_LEVEL}` | Build showcase sandbox game (`game/`). |
| `MGE_SOURCE_ASSET_FALLBACK` | `ON` | Embed absolute path to source `assets/` directory in binaries as last-resort fallback (`OFF` for distributable packages). |
| `MGE_WARNINGS` | `ON` | Enable strict compiler warnings (`-Wall -Wextra -Wshadow`). |
| `MGE_WERROR` | `OFF` | Treat compiler warnings as errors. |
| `MGE_PROFILING` | `ON` | Enable CPU profiler scope instrumentation (`MGE_PROFILE_SCOPE`). |
| `MGE_TRACY` | `OFF` | Enable Tracy profiler integration. |

---

## 6. Runtime Assets

By default, games resolve assets in this order:
1. `MGE_ASSET_DIR` environment variable (if set and pointing to a directory).
2. `<exe_dir>/assets` or `<exe_dir>/../assets`.
3. If built in-tree with `MGE_SOURCE_ASSET_FALLBACK=ON`, `${PROJECT_SOURCE_DIR}/assets`.
