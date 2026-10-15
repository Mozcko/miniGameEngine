# Building and running

## Requirements

| Dependency | Used for | Found by CMake? | Fedora package | Debian/Ubuntu package |
|------------|----------|-----------------|----------------|-----------------------|
| CMake ≥ 3.10 | build | – | `cmake` | `cmake` |
| C++17 compiler | build | – | `gcc-c++` | `g++` |
| SDL3 ≥ 3.2 | window, events, input, audio | `FetchContent` with `FIND_PACKAGE_ARGS`: system package if installed, otherwise **built from source** (pinned 3.4.18, SHA‑256) | `SDL3-devel` | not packaged in 24.04 → built from source |
| Vulkan loader + driver | SDL3 GPU backend (`mge_gpu`) | loaded at runtime by SDL | `vulkan-loader` + `mesa-vulkan-drivers` | `libvulkan1` + `mesa-vulkan-drivers` |
| glslang | GLSL → SPIR‑V shader compiler (build time, #29) | `find_program(glslang / glslangValidator)`; if missing, **built from source** (pinned 16.2.0, SHA‑256) | `glslang` | `glslang-tools` |
| Vulkan validation layers (optional) | GPU API error checking in Debug builds | loaded at runtime | `vulkan-validation-layers` | `vulkan-validationlayers` |
| GLEW | OpenGL loader (legacy renderer, removed in #40) | `find_package(GLEW)` | `glew-devel` | `libglew-dev` |
| OpenGL 3.3 | rendering | `find_package(OpenGL)` | mesa / GPU driver | `libgl-dev` |
| GLM | math (header‑only) | **no** – must be on the default include path | `glm-devel` | `libglm-dev` |
| stb_image | PNG loading (header‑only) | **no** – included as `<stb/stb_image.h>` | `stb_image-devel` (or `stb-devel`) | `libstb-dev` |

`STB_IMAGE_IMPLEMENTATION` is defined in exactly one place:
`src/mge/render/core/Texture.cpp`. Don't define it anywhere else.

## Build

Use the CMake presets (CMake ≥ 3.21):

```bash
cmake --preset dev && cmake --build --preset dev        # Debug + warnings      → build/
cmake --preset release && cmake --build --preset release # Release              → build-release/
cmake --preset ci && cmake --build --preset ci          # Debug + -Werror (CI)  → build-ci/
cmake --preset asan && cmake --build --preset asan      # Debug + ASan/UBSan    → build-asan/
```

A plain `cmake -S . -B build && cmake --build build` also works and defaults to **Release**.

| Option | Default | Effect |
|--------|---------|--------|
| `CMAKE_BUILD_TYPE` | `Release` | Set automatically for single‑config generators when empty |
| `MGE_WARNINGS` | `ON` | `-Wall -Wextra -Wshadow` (`/W4` on MSVC), applied to our code only |
| `MGE_WERROR` | `OFF` | Treat warnings as errors (`ON` in the `ci` preset) |
| `MGE_BUILD_TESTS` | `ON` | Build the test executables and register them with CTest |
| `MGE_SANITIZERS` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer for the whole build (`asan` preset). UB aborts (`-fno-sanitize-recover`) |
| `MGE_BUILD_BENCHMARKS` | `ON` | Build `bench/` (e.g. `ecs_bench`; run from a Release build) |
| `MGE_BUILD_MODULES` | `ON` | `OFF` = only `mge_core` (no SDL/GPU dependencies) |

- Every preset's build dir is a sibling of `assets/`, so the game's `../assets/...` paths work
  from any of them. `build/` and `build-*/` are git‑ignored.
- `compile_commands.json` is generated in the build dir (for clangd / clang‑tidy).
- The build must stay **warning‑free** on GCC and Clang.
- Targets:

  | Target | Kind | Defined in | Contents |
  |--------|------|------------|----------|
  | `mge_core` | static lib | `src/mge/core/` | Headless core – **no SDL/GPU** (std + GLM): Application runner, Module, Clock/Time, Input, Random, ECS, Log |
  | `mge_procgen` | static lib | `src/mge/procgen/` | Procedural generation, headless (always built) |
  | `mge_voxel` | static lib | `src/mge/voxel/` | Voxel worlds, headless (always built) |
  | `mge_platform` | static lib | `src/mge/platform/` | `PlatformModule`: window, GL context, input |
  | `mge_gpu` | static lib | `src/mge/gpu/` | SDL3 GPU (Vulkan): `GpuModule`, RAII handles, mesh upload |
  | `mge_graphics` | static lib | `src/mge/graphics/` | Renderer on SDL GPU: `Texture2D`, `MeshRenderer` |
  | `mge_render` | static lib | `src/mge/render/` | OpenGL renderer (legacy, removed in #40; only its cameras are still used) |
  | `mge_audio` | static lib | `src/mge/audio/` | `AudioModule` |
  | `mge_main` | OBJECT lib | `src/mge/app/` | `main()` → `createApplication()` → `run()` (core‑only) |
  | `sandbox_world` | static lib | `game/` | Sandbox blocks + terrain generator (headless, on `mge_voxel`) |
  | `sandbox` | executable | `game/` | The game; output in the build dir root |
  | `mge_core_tests`, `mge_procgen_tests`, `mge_voxel_tests` | executables | `tests/` | Headless tests |
  | `mge_platform_tests`, `mge_gpu_tests`, `game_tests` | executables | `tests/` | Platform, GPU (label `gpu`, offscreen) and sandbox tests |
  | `ecs_bench`, `noise_bench` | executables | `bench/` | ECS and noise benchmarks (run from a Release build) |

  Every target lists its sources explicitly in its own `CMakeLists.txt`: add new files there.
- `-DMGE_BUILD_MODULES=OFF` builds only the headless targets (`mge_core`, `mge_procgen`, `mge_voxel`, their tests and `procgen_gallery`). No SDL, GLEW or OpenGL is
  needed; this is how CI proves the core is headless.
- Include roots are `src/` (`#include <mge/core/Time.h>`) and the repo root, for the game (`#include <game/SandboxTerrain.h>`).
- OpenGL is linked through GLVND (`OpenGL::GL`).

## SDL3: system package or built from source

`CMakeLists.txt` declares SDL3 with `FetchContent_Declare(... FIND_PACKAGE_ARGS 3.2 CONFIG)`:

- If an SDL3 ≥ 3.2 CMake package is installed (Fedora: `sudo dnf install SDL3-devel`), it is
  used. This is the fastest option.
- Otherwise SDL **3.4.18** is downloaded (SHA‑256 pinned) and built as a static library inside the
  build dir. That is how CI works on Ubuntu 24.04, which doesn't package SDL3. The configure step
  prints which path was taken (`SDL3: usando la instalación del sistema` / `compilado desde el
  código fuente`).
- Building from source needs SDL's Linux development headers (X11, Wayland, EGL/GL, xkbcommon,
  audio backends). CI installs them in `scripts/ci-install-deps.sh`; the full list is in SDL's
  [README-linux](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies).
  - The XScrnSaver extension is disabled (`SDL_X11_XSCRNSAVER=OFF`), so its package isn't needed.
  - Without libdecor (`libdecor-devel` / `libdecor-0-dev`), SDL still builds, but windows on GNOME
    Wayland have no title bar.
- To force a source build even with SDL3 installed:
  `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`.

## Sanitizers

```bash
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
cd build-asan && SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout -s INT 10 ./sandbox
```

GCC needs its ASan/UBSan runtimes (Fedora: `sudo dnf install libasan libubsan`). Clang ships them:
`CC=clang CXX=clang++ cmake --preset asan`. The test preset sets `ASAN_OPTIONS=detect_leaks=1` and
makes UBSan halt on the first error.

## Headless runs

SDL3's offscreen video driver plus GLEW on EGL lets the game run without a display (useful in CI
and over SSH):

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy MGE_MAX_FRAMES=300 ./build/sandbox
```

`MGE_MAX_FRAMES=N` makes any engine application exit cleanly after N frames, unless the game set
`ApplicationConfig::maxFrames` itself.

## Shaders

Shaders are GLSL (Vulkan dialect), compiled to SPIR‑V **at build time** and embedded in the
executable, so no shader file is read at runtime:

```cmake
mge_add_shaders(my_game SHADERS shaders/mesh.vert shaders/mesh.frag)   # cmake/MgeShaders.cmake
```

```cpp
#include <shaders/mesh_vert.h>          // generated: mge::shaders::kMeshVert (a mge::ShaderCode)
mge::ShaderResources resources;
resources.uniformBuffers = 1;
mge::GpuShader vs = device.createShader(mge::shaders::kMeshVert, resources);
```

- **Stage** comes from the extension: `.vert`, `.frag` or `.comp`.
- **Rebuilds:** editing a shader recompiles only that shader and the files that include its
  header. `#include` inside GLSL isn't supported yet.
- **Binding convention** (required by SDL GPU), detailed in `src/mge/gpu/Shader.h`:

  | Stage | Textures and buffers | Uniform buffers |
  |-------|----------------------|-----------------|
  | vertex | `set 0` | `set 1` |
  | fragment | `set 2` | `set 3` |

- **Compiler:** the system `glslang` if installed, otherwise glslang is built from source once
  (~1–2 min).
- **Linting:** `scripts/lint.sh` builds the `mge_generated_shaders` target before clang‑tidy,
  because the analysis needs the generated headers.

## Adding a game

```bash
scripts/new-game.sh space_shooter              # window template  → examples/space_shooter/
scripts/new-game.sh simulation --headless      # headless template
cmake --preset dev                             # pick up the new folder
cmake --build --preset dev --target run-space_shooter
```

Every program is declared with **`mge_add_game()`** (`cmake/MgeGame.cmake`):

```cmake
mge_add_game(<name>
    SOURCES <files...>             # required
    [LIBRARIES <libs...>]          # extra game libraries
    [MODULES platform gpu graphics audio]   # engine modules (this is the default; `render` = legacy GL)
    [HEADLESS]                     # no modules: no window, GPU or SDL
    [SMOKE_FRAMES <n>])            # CTest smoke test running n frames (windowed → offscreen, label gpu)
```

It links `mge_main` and the modules, applies the warnings, puts the executable in the build‑dir
root, and adds a `run-<name>` target. `run` points at `MGE_MAIN_GAME` (default `sandbox`). See
[`templates/README.md`](../templates/README.md).

## Tests

Unit tests use [doctest](https://github.com/doctest/doctest) (fetched by CMake, pinned + hash‑checked, never linked into the game) and run through CTest:

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev            # or: ctest --test-dir build --output-on-failure
```

Tests are **headless**: they need no display. Tests labelled `gpu` need a GPU driver: Vulkan for
`mge_gpu_tests`, `render_tests` and the SDL GPU smoke tests, OpenGL for the legacy sandbox smoke test.
They render offscreen and are **skipped** when no GPU is found, unless `MGE_REQUIRE_GPU=1`, as in
CI, where Mesa lavapipe provides software Vulkan. With the validation layers installed, any `Validation Error`
in a GPU test's output fails it (CTest `FAIL_REGULAR_EXPRESSION`). That catches API misuse and leaked GPU
objects. CI also sets `MGE_REQUIRE_VALIDATION=1`, and `gpu_validation_probe` proves the mechanism works. Skip them all with
`ctest -LE gpu`. Disable tests with `-DMGE_BUILD_TESTS=OFF`. See `tests/README.md`.

**Render tests (golden images):** `render_tests` draws scenes offscreen and compares them with PNGs
in `tests/render/golden/`. A comparison passes when at most 1 % of pixels differ by more than 8/255
in any channel; the tolerance covers driver differences at triangle edges.
- After an intended visual change: `MGE_UPDATE_GOLDEN=1 ./build/tests/render_tests`, then **look
  at the new PNGs** before committing them.
- On failure, the rendered image and a red diff image are written to `<build>/tests/render_output/`.

## Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and on every pull request (Ubuntu 24.04;
superseded runs are cancelled):

| Job | What it proves |
|-----|----------------|
| `gcc`, `clang` | `ci` preset (Debug, `-Werror`) builds; all tests pass headless, including both smoke tests (the sandbox runs 120 frames offscreen on Mesa llvmpipe) |
| `sanitizers` | `asan` preset: the same tests and smoke runs under ASan + UBSan, with no display server |
| `core-only` | `mge_core` builds and its tests pass on a machine **without** SDL/GL headers |
| `lint` | `scripts/lint.sh`: clang-format and naming (22.1.8) |

Dependencies come from `scripts/ci-install-deps.sh` (SDL3 build deps plus Mesa software drivers).

A PR may only be merged when both jobs are green. To reproduce CI locally:
`cmake --preset ci -G Ninja && cmake --build --preset ci && ctest --preset ci`.

## Run

```bash
scripts/run.sh [preset] [-- game args]   # configure if needed + build + run (default preset: dev)
cmake --build --preset dev --target run  # same, as a CMake target
./build/sandbox                          # the binary itself, from any directory
```

The game no longer depends on the working directory. `mge::assetPath("shaders/x.vert")`
(`<mge/core/Assets.h>`) resolves the asset root once, logs it, and searches in this order:
1. the `MGE_ASSET_DIR` environment variable;
2. `<exe dir>/assets`;
3. `<exe dir>/../assets`, the normal case (`build*/sandbox` → `assets/`);
4. the source tree's `assets/`, whose absolute path is baked in at configure time. This means a
   build dir placed anywhere still works.

Useful environment variables: `MGE_MAX_FRAMES=N` (exit after N frames), `MGE_VSYNC=0|1` (override
`ApplicationConfig::vsync`), `MGE_GPU_DEBUG=0|1` (GPU validation on/off; default on in Debug), `MGE_ASSET_DIR`,
`SDL_VIDEODRIVER=offscreen`, `SDL_AUDIODRIVER=dummy`.

Expected console output at startup (engine log on stderr: `[seconds] [LEVEL] [Category] message`):
```
[    0.000] [INFO ] [Audio] Sistema de audio inicializado
[    0.061] [INFO ] [Window] OpenGL <driver string>
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `[Shader] No se pudo abrir el archivo: …` then `[Sandbox] No se pudo crear el shader 3D. Assets en: …` | The asset root the log shows is wrong: set `MGE_ASSET_DIR=/path/to/assets`, or check the `[Assets] Raíz de assets` line |
| `[Texture] No se pudo cargar la textura ...` | Same as above, or PNG missing. |
| SDL configure error `Couldn't find dependency package for …` | Building SDL3 from source without one of its dev packages: install it (see above) or `sudo dnf install SDL3-devel` to skip the source build. |
| `[Window] No se pudo crear el contexto OpenGL` / `No se pudo inicializar GLEW` | GPU/driver doesn't provide GL 3.3 core (e.g. some VMs). Try `LIBGL_ALWAYS_SOFTWARE=1`. |
| `[Audio] No se pudo abrir el dispositivo de audio` | No audio device; non‑fatal, game continues silently. |
| `fatal error: stb/stb_image.h` / `glm/glm.hpp` not found | Install the header packages above, or add their include dir to `include_directories()` in CMakeLists. |
| Mouse can't escape window | Cursor is locked by design. Press `P` to pause (releases it) or `Esc` to quit. |

## VS Code

Open the folder and install the recommended extensions (**CMake Tools**, **C/C++**, CMake syntax;
VS Code suggests them from `.vscode/extensions.json`). CMake Tools then configures with the `dev`
preset.

| Want to… | Do |
|----------|----|
| Build | **Ctrl+Shift+B** ("Build (dev)", configures first if needed) |
| Run the game | Terminal → Run Task → **Run game** (or "Run game (offscreen, 300 frames)") |
| Debug the game | **F5** → "Debug sandbox" (gdb, builds first). "Debug sandbox (offscreen, 300 frames)" needs no window |
| Debug tests | F5 → "Debug core tests" / "Debug game tests", then enter a doctest filter (e.g. `*chunk*`, or `*` for all) |
| Run tests | Run Task → **Test (dev)** (default test task) or **Test (ASan + UBSan)**; CMake Tools' Testing panel also lists every CTest test |
| Lint / format | Run Task → **Lint (format + naming)** (`scripts/lint.sh --fix`); files are also formatted on save with `.clang-format` |
| New game | Run Task → **New game** (asks for a name and template, then reconfigures) |

IntelliSense takes include paths and flags from CMake Tools. `build*/` folders are hidden from the
explorer and from search.

## The `build/` folder

`build/` is git-ignored (#1). It used to be committed, binary included; it was removed from the index with `git rm -r --cached build`.
