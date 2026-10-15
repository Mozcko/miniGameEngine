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

```bash
cmake -S . -B build              # configure (add -DCMAKE_BUILD_TYPE=Debug for symbols)
cmake --build build -j           # compile
```

- Sources are collected with `file(GLOB_RECURSE …)` over `src/Engine` and `src/Game`. After
  **adding or removing** a `.cpp`/`.h` file, re‑run the configure step.
- Include root is `src/`, so `#include <Engine/Core/Application.h>` works from anywhere.
- The only expected output noise is a CMake `CMP0072` developer warning about OpenGL GLVND vs.
  legacy libGL. Harmless; silence it with `set(OpenGL_GL_PREFERENCE GLVND)` before
  `find_package(OpenGL)` if wanted.

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
