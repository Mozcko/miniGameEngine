# Building and running

## Requirements

| Dependency | Used for | Found by CMake? | Fedora package | Debian/Ubuntu package |
|------------|----------|-----------------|----------------|-----------------------|
| CMake ≥ 3.10 | build | – | `cmake` | `cmake` |
| C++17 compiler | build | – | `gcc-c++` | `g++` |
| SDL3 ≥ 3.2 | window, events, input, audio | `FetchContent` with `FIND_PACKAGE_ARGS`: system package if installed, otherwise **built from source** (pinned 3.4.18, SHA‑256) | `SDL3-devel` | not packaged in 24.04 → built from source |
| GLEW | OpenGL loader | `find_package(GLEW)` | `glew-devel` | `libglew-dev` |
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
| `MGE_BUILD_MODULES` | `ON` | `OFF` = only `mge_core` (no SDL/GPU dependencies) |

- Every preset's build dir is a sibling of `assets/`, so the game's `../assets/...` paths work
  from any of them. `build/` and `build-*/` are git‑ignored.
- `compile_commands.json` is generated in the build dir (for clangd / clang‑tidy).
- The build must stay **warning‑free** on GCC and Clang.
- Targets:

  | Target | Kind | Contents |
  |--------|------|----------|
  | Target | Kind | Defined in | Contents |
  |--------|------|------------|----------|
  | `mge_core` | static lib | `src/mge/core/` | Headless core – **no SDL/GPU**: Application runner, Module, Log, Time |
  | `mge_platform` | static lib | `src/mge/platform/` | `PlatformModule`: window, GL context, input |
  | `mge_render` | static lib | `src/mge/render/` | OpenGL renderer |
  | `mge_audio` | static lib | `src/mge/audio/` | `AudioModule` |
  | `mge_main` | OBJECT lib | `src/mge/app/` | `main()` → `createApplication()` → `run()` (core‑only) |
  | `sandbox_world` | static lib | `games/sandbox/` | `Block`, `Chunk` (testable game logic) |
  | `sandbox` | executable | `games/sandbox/` | The game; output in the build dir root |
  | `mge_core_tests` | executable | `tests/` | Core tests – link only `mge_core` |
  | `sandbox_tests` | executable | `tests/` | Sandbox tests |

  Every target lists its sources explicitly in its own `CMakeLists.txt`: add new files there.
- `-DMGE_BUILD_MODULES=OFF` builds only `mge_core` + `mge_core_tests`. No SDL, GLEW or OpenGL is
  needed; this is how CI proves the core is headless.
- Include roots are `src/` (`#include <mge/core/Time.h>`) and `games/` (`#include <sandbox/Chunk.h>`).
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
cd build && SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy MGE_MAX_FRAMES=300 ./sandbox
```

`MGE_MAX_FRAMES=N` makes any engine application exit cleanly after N frames, unless the game set
`ApplicationConfig::maxFrames` itself.

## Tests

Unit tests use [doctest](https://github.com/doctest/doctest) (fetched by CMake, pinned + hash‑checked, never linked into the game) and run through CTest:

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev            # or: ctest --test-dir build --output-on-failure
```

Tests are **headless**: they need no display. Only the `gpu`‑labelled smoke test needs an OpenGL
driver; skip it with `ctest -LE gpu`. Disable them with `-DMGE_BUILD_TESTS=OFF`. See `tests/README.md`.

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
cd build
./sandbox
```

**The working directory must be `build/`** (or any directory that is a sibling of `assets/`):
every asset is loaded with a hard‑coded relative path like
`"../assets/shaders/basic3d.vert"`. Running from the repo root produces
`No se pudo abrir el archivo: ../assets/shaders/basic3d.vert` and the program exits.

Expected console output at startup (engine log on stderr: `[seconds] [LEVEL] [Category] message`):
```
[    0.000] [INFO ] [Audio] Sistema de audio inicializado
[    0.061] [INFO ] [Window] OpenGL <driver string>
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `[Shader] No se pudo abrir el archivo: ../assets/...` then `[Sandbox] No se pudo crear el shader 3D` and a clean exit | Wrong working directory – `cd build` first. |
| `[Texture] No se pudo cargar la textura ...` | Same as above, or PNG missing. |
| `[GUI] No se pudo cargar la fuente ...` | Font path wrong (cwd). |
| SDL configure error `Couldn't find dependency package for …` | Building SDL3 from source without one of its dev packages: install it (see above) or `sudo dnf install SDL3-devel` to skip the source build. |
| `[Window] No se pudo crear el contexto OpenGL` / `No se pudo inicializar GLEW` | GPU/driver doesn't provide GL 3.3 core (e.g. some VMs). Try `LIBGL_ALWAYS_SOFTWARE=1`. |
| `[Audio] No se pudo abrir el dispositivo de audio` | No audio device; non‑fatal, game continues silently. |
| `fatal error: stb/stb_image.h` / `glm/glm.hpp` not found | Install the header packages above, or add their include dir to `include_directories()` in CMakeLists. |
| Mouse can't escape window | Cursor is locked by design. Press `P` to pause (releases it) or `Esc` to quit. |

## Editor notes

- `.vscode/tasks.json` contains only an auto‑generated "compile active file with gcc" task. It
  does **not** build this project (it's single‑file C, not CMake). Use the CMake commands above or
  the CMake Tools extension.
- `.vscode/settings.json` only tweaks minimap/scrollbar colours.

## The `build/` folder

`build/` is git-ignored (#1). It used to be committed, binary included; it was removed from the index with `git rm -r --cached build`.
