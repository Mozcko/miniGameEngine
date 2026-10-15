# src/

All C++ source code. Compiled into one executable (`miniGameEngine`) by the root
`CMakeLists.txt`, which globs every `.cpp`/`.h` under `src/Engine/` and `src/Game/`.

| Folder | Role |
|--------|------|
| [`Engine/`](Engine/README.md) | Reusable engine: entry point, application loop, window, input, audio, rendering. Knows nothing about the game. |
| [`Game/`](Game/README.md) | The client application (`SandboxApp`) – a Minecraft‑style voxel sandbox built on the engine. |

## Dependency direction

`Game` → `Engine` only. The single "upward" link is the function
`Engine::CreateApplication()`, which is **declared** in `Engine/Core/Application.h` and
**defined** in `Game/SandboxApp.cpp`; `Engine/Main.cpp` calls it.

## Include conventions

- `src/` is on the include path (`include_directories(src)`), so both styles work:
  - `#include <Engine/Core/Application.h>` (root‑relative), and
  - `#include "../Engine/Graphics/3D/Mesh.h"` (file‑relative).
  Inside `Engine/` files mostly use file‑relative includes.
- Third‑party headers: `<SDL2/SDL.h>`, `<SDL2/SDL_mixer.h>`, `<SDL2/SDL_ttf.h>`, `<GL/glew.h>`,
  `<glm/...>`, `<stb/stb_image.h>`.
- All headers use `#pragma once`.

## Namespaces

| Namespace | Contains |
|-----------|----------|
| `Engine` | `Application`, `CreateApplication`, `InputManager`, `Camera`, `Camera2D`, `Camera3D`, `FirstPersonCamera` |
| `MiniGameEngine` | `Window`, `Time`, `ResourceManager`, `AudioManager`, `Vertex`, `Texture`, `GLSLProgram`, `Mesh`, `Sprite`, `Animation`, `GUILabel`, and the game's `Block`, `BlockType`, `UVRect`, `Chunk`, `CHUNK_SIZE`, `CHUNK_HEIGHT` |
| global | `SandboxApp`, `ChunkKey` (in `SandboxApp.cpp`, which does `using namespace MiniGameEngine;`) |

## Style

- Comments are in Spanish.
- `Engine::` classes: `PascalCase` methods, `m_` / `s_` member prefixes.
- `MiniGameEngine::` classes: `camelCase` methods, `_` member prefix.
- Match the style of the file you're editing.
