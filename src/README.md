# src/

Engine source code. Games live outside, in [`games/`](../games/), and tests in
[`tests/`](../tests/README.md).

```
src/
└── mge/                 the engine (one folder per CMake target)  → mge/README.md
    ├── core/            mge_core      – headless: no SDL, no GPU
    ├── platform/        mge_platform  – SDL window, GL context, input
    ├── render/          mge_render    – OpenGL renderer (legacy until #40)
    ├── audio/           mge_audio     – audio
    └── app/             mge_main – main() (core only)
```

## Dependency direction

```
games/sandbox ──► mge_main ──────────────────► mge_core (Application runner, Module)
      ├─────────────────────► mge_platform ──► mge_core   (PlatformModule)
      ├─────────────────────► mge_audio ─────► mge_core   (AudioModule)
      └─────────────────────► mge_render ────► mge_core (+ mge_platform, temporary)
```

- `mge_core` depends on nothing but the standard library. The CI `core-only` job proves it by
  building with `-DMGE_BUILD_MODULES=OFF` on a machine without SDL.
- The single "upward" link is `mge::createApplication()`: it is **declared** in
  `mge/core/Application.h` and **defined** by the game; `mge/app/Main.cpp` calls it.
- **Temporary:** `mge_render` links `mge_platform` because the cameras read the static
  `InputManager`. Removed in #15, when input moves into the core.

## Include conventions

- `src/` is the include root for the engine and `games/` for games. Always include
  root‑relative:
  - `#include <mge/core/Time.h>`
  - `#include <mge/render/3d/Mesh.h>`
  - `#include <sandbox/Chunk.h>`
- Third‑party headers: `<SDL3/...>`, `<GL/glew.h>`, `<glm/...>`, `<stb/stb_image.h>`.
- All headers use `#pragma once`.
- Every target lists its sources explicitly in its own `CMakeLists.txt`. Add new files there.

## Namespace & naming

Everything is in namespace **`mge`**. Naming and formatting follow
[CONTRIBUTING.md](../CONTRIBUTING.md#code-conventions) (`camelCase` methods, `m_` members,
`kPascalCase` constants) and are checked by the CI `lint` job (`.clang-format`, `.clang-tidy`).

## Style
Comments are in Spanish. Naming and conventions are in [CONTRIBUTING.md](../CONTRIBUTING.md).
