# miniGameEngine

A small, educational C++17 game engine built on **SDL2 + OpenGL 3.3 Core (via GLEW)**, with a
Minecraft‑style voxel sandbox as its demo game.

The engine owns `main()` and the game loop. A game plugs in by subclassing
`Engine::Application` and defining `Engine::CreateApplication()` (the "Hazel‑style" entry‑point
pattern). The current client is `SandboxApp` (`src/Game/SandboxApp.cpp`): it streams in procedurally
generated 16×16×16 block chunks around a free‑flying, collision‑aware camera, animates
water/lava tiles, and draws a 2D text HUD.

> Source code comments are written in **Spanish**. The documentation is in English.

**Where it's heading:** the engine becomes a headless C++ core with optional modules, built on
SDL3 and the SDL3 GPU API (Vulkan/Metal/D3D12). Games will be made purely from code: procedural
geometry, shaders and audio, shipped as a single executable with no asset files.
See **[docs/ROADMAP.md](docs/ROADMAP.md)** and the
[milestones](https://github.com/Mozcko/miniGameEngine/milestones).

---

## Quick start

```bash
# Fedora (adjust package names for your distro – see docs/BUILDING.md)
sudo dnf install cmake gcc-c++ SDL2-devel SDL2_mixer-devel SDL2_ttf-devel glew-devel glm-devel stb_image-devel

cmake --preset release          # or: cmake -S . -B build (defaults to Release)
cmake --build --preset release
cd build-release && ./miniGameEngine   # MUST run from a dir next to assets/ ("../assets/...")
```

### Controls (SandboxApp)

| Input                | Action                                              |
|----------------------|-----------------------------------------------------|
| Mouse                | Look around (cursor is locked / relative mode)      |
| `W` `A` `S` `D`      | Move forward / left / back / right (free‑fly)       |
| `Space` / `L‑Shift`  | Move up / down                                      |
| `L‑Ctrl` (hold)      | Sprint (×2.5)                                       |
| `P`                  | Pause / resume (time scale 0, releases the cursor)  |
| Mouse click          | Re‑capture the cursor (when not paused)             |
| `Esc`                | Quit                                                |

---

## Repository layout

```
miniGameEngine/
├── CMakeLists.txt          Build script: one executable from src/Engine + src/Game
├── CLAUDE.md               Condensed context for AI assistants / new contributors
├── README.md               This file
├── LICENSE                 GNU LGPL v2.1
├── docs/                   Cross‑cutting documentation
│   ├── ROADMAP.md          Goals, decisions, milestones → GitHub issues
│   ├── ARCHITECTURE.md     Main loop, namespaces, rendering pipeline, ownership
│   ├── BUILDING.md         Dependencies, build/run, troubleshooting
│   └── KNOWN_ISSUES.md     Bugs, quirks and technical debt found while reviewing
├── src/                    All C++ code              → src/README.md
│   ├── Engine/             Reusable engine           → src/Engine/README.md
│   │   ├── Main.cpp        Program entry point
│   │   ├── Core/           Application, Time, ResourceManager
│   │   ├── Window/         SDL window + GL context + GLEW init
│   │   ├── Input/          Keyboard / mouse state
│   │   ├── Audio/          SDL_mixer wrapper (SFX + music)
│   │   └── Graphics/       Rendering                 → src/Engine/Graphics/README.md
│   │       ├── Core/       Vertex, Texture, GLSLProgram
│   │       ├── 3D/         Mesh (VAO/VBO/EBO)
│   │       ├── 2D/         Sprite, Animation, GUILabel
│   │       └── Camera/     Camera, Camera2D, Camera3D, FirstPersonCamera
│   └── Game/               The voxel sandbox client  → src/Game/README.md
├── assets/                 Runtime data              → assets/README.md
│   ├── shaders/            GLSL 330 shaders
│   ├── textures/           512×512 block tileset atlas
│   ├── fonts/              Pixel fonts for the HUD
│   ├── sounds/             WAV sound effects (unused by current game)
│   ├── music/              MP3 music (unused by current game)
│   └── levels/             ASCII 2D level files (legacy, no loader exists)
├── build/                  CMake build dir (git-ignored)
└── .vscode/                Editor settings + a single‑file gcc task (not the CMake build)
```

Every folder under `src/` and `assets/` has its own `README.md` describing each file in detail.

## Documentation index

| Document | What it covers |
|----------|----------------|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Goals, key decisions, milestones and GitHub issues |
| [CLAUDE.md](CLAUDE.md) | Short "everything you need to know" summary and conventions |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the pieces fit together at runtime |
| [docs/BUILDING.md](docs/BUILDING.md) | Toolchain, dependencies, running, troubleshooting |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | Bugs and debt, with file references |
| [src/README.md](src/README.md) | Source tree overview, include/namespace conventions |
| [src/Engine/README.md](src/Engine/README.md) | Engine module map |
| [src/Engine/Core/README.md](src/Engine/Core/README.md) | Application, Time, ResourceManager |
| [src/Engine/Window/README.md](src/Engine/Window/README.md) | Window |
| [src/Engine/Input/README.md](src/Engine/Input/README.md) | InputManager |
| [src/Engine/Audio/README.md](src/Engine/Audio/README.md) | AudioManager |
| [src/Engine/Graphics/README.md](src/Engine/Graphics/README.md) | Graphics overview |
| [src/Engine/Graphics/Core/README.md](src/Engine/Graphics/Core/README.md) | Vertex, Texture, GLSLProgram |
| [src/Engine/Graphics/3D/README.md](src/Engine/Graphics/3D/README.md) | Mesh |
| [src/Engine/Graphics/2D/README.md](src/Engine/Graphics/2D/README.md) | Sprite, Animation, GUILabel |
| [src/Engine/Graphics/Camera/README.md](src/Engine/Graphics/Camera/README.md) | Camera hierarchy |
| [src/Game/README.md](src/Game/README.md) | SandboxApp, Chunk, Block (voxel world) |
| [assets/README.md](assets/README.md) | Asset overview and path rules |

## License

GNU Lesser General Public License v2.1 – see [LICENSE](LICENSE).
