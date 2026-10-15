# Building and running

## Requirements

| Dependency | Used for | Found by CMake? | Fedora package | Debian/Ubuntu package |
|------------|----------|-----------------|----------------|-----------------------|
| CMake ≥ 3.10 | build | – | `cmake` | `cmake` |
| C++17 compiler | build | – | `gcc-c++` | `g++` |
| SDL2 | window, events, input, timer | `find_package(SDL2)` | `SDL2-devel` | `libsdl2-dev` |
| SDL2_mixer | audio | `find_package(SDL2_mixer)` | `SDL2_mixer-devel` | `libsdl2-mixer-dev` |
| SDL2_ttf | font rendering | `find_package(SDL2_ttf)` | `SDL2_ttf-devel` | `libsdl2-ttf-dev` |
| GLEW | OpenGL loader | `find_package(GLEW)` | `glew-devel` | `libglew-dev` |
| OpenGL 3.3 | rendering | `find_package(OpenGL)` | mesa / GPU driver | `libgl-dev` |
| GLM | math (header‑only) | **no** – must be on the default include path | `glm-devel` | `libglm-dev` |
| stb_image | PNG loading (header‑only) | **no** – included as `<stb/stb_image.h>` | `stb_image-devel` (or `stb-devel`) | `libstb-dev` |

`STB_IMAGE_IMPLEMENTATION` is defined in exactly one place:
`src/Engine/Graphics/Core/Texture.cpp`. Don't define it anywhere else.

## Build

Use the CMake presets (CMake ≥ 3.21):

```bash
cmake --preset dev && cmake --build --preset dev        # Debug + warnings      → build/
cmake --preset release && cmake --build --preset release # Release              → build-release/
cmake --preset ci && cmake --build --preset ci          # Debug + -Werror (CI)  → build-ci/
```

A plain `cmake -S . -B build && cmake --build build` also works and defaults to **Release**.

| Option | Default | Effect |
|--------|---------|--------|
| `CMAKE_BUILD_TYPE` | `Release` | Set automatically for single‑config generators when empty |
| `MGE_WARNINGS` | `ON` | `-Wall -Wextra -Wshadow` (`/W4` on MSVC), applied to our code only |
| `MGE_WERROR` | `OFF` | Treat warnings as errors (`ON` in the `ci` preset) |
| `MGE_BUILD_TESTS` | `ON` | Build `mge_tests` and register it with CTest |

- Every preset's build dir is a sibling of `assets/`, so the game's `../assets/...` paths work
  from any of them. `build/` and `build-*/` are git‑ignored.
- `compile_commands.json` is generated in the build dir (for clangd / clang‑tidy).
- The build must stay **warning‑free** on GCC and Clang.
- Targets:

  | Target | Kind | Contents |
  |--------|------|----------|
  | `mge_engine` | static lib | `src/Engine/**` except `Main.cpp` (globbed) |
  | `sandbox_world` | static lib | `src/Game/Block.*`, `src/Game/Chunk.*` (listed explicitly) |
  | `miniGameEngine` | executable | `src/Engine/Main.cpp` + `src/Game/SandboxApp.cpp` |
  | `mge_tests` | executable | `tests/*.cpp` |

  Engine sources are globbed: after **adding or removing** an engine file, re‑run the configure
  step. New game files must be added to their target in `CMakeLists.txt`. The full module split is #8.
- Include root is `src/`, so `#include <Engine/Core/Application.h>` works from anywhere.
- OpenGL is linked through GLVND (`OpenGL::GL`).

## Tests

Unit tests use [doctest](https://github.com/doctest/doctest) (fetched by CMake, pinned + hash‑checked, never linked into the game) and run through CTest:

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev            # or: ctest --test-dir build --output-on-failure
```

Tests are **headless**: they need no display or GPU. Disable them with `-DMGE_BUILD_TESTS=OFF`. See `tests/README.md`.

## Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and on every pull request:
- Ubuntu 24.04, **GCC and Clang** matrix, Ninja, ccache.
- `cmake --preset ci` (Debug, `-Werror`) → build → `ctest --preset ci` (headless).
- Superseded runs of the same branch are cancelled.

A PR may only be merged when both jobs are green. To reproduce CI locally:
`cmake --preset ci -G Ninja && cmake --build --preset ci && ctest --preset ci`.

## Run

```bash
cd build
./miniGameEngine
```

**The working directory must be `build/`** (or any directory that is a sibling of `assets/`):
every asset is loaded with a hard‑coded relative path like
`"../assets/shaders/basic3d.vert"`. Running from the repo root produces
`No se pudo abrir el archivo: ../assets/shaders/basic3d.vert` and the program exits.

Expected console output at startup:
```
[AudioManager] Sistema de audio inicializado.
OpenGL Version: <driver string>
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `No se pudo abrir el archivo: ../assets/...` then exit | Wrong working directory – `cd build` first. |
| `Error: No se pudo cargar la textura ...` | Same as above, or PNG missing. |
| `[GUILabel] Error cargando fuente` | Font path wrong (cwd). |
| `Error al crear contexto OpenGL` | GPU/driver doesn't provide GL 3.3 core (e.g. some VMs). Try `LIBGL_ALWAYS_SOFTWARE=1`. |
| `[AudioManager] Error fatal al iniciar SDL_Mixer` | No audio device; non‑fatal, game continues. |
| `fatal error: stb/stb_image.h` / `glm/glm.hpp` not found | Install the header packages above, or add their include dir to `include_directories()` in CMakeLists. |
| Mouse can't escape window | Cursor is locked by design. Press `P` to pause (releases it) or `Esc` to quit. |

## Editor notes

- `.vscode/tasks.json` contains only an auto‑generated "compile active file with gcc" task. It
  does **not** build this project (it's single‑file C, not CMake). Use the CMake commands above or
  the CMake Tools extension.
- `.vscode/settings.json` only tweaks minimap/scrollbar colours.

## The `build/` folder

`build/` is git-ignored (#1). It used to be committed, binary included; it was removed from the index with `git rm -r --cached build`.
