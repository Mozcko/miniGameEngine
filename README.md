# miniGameEngine

[![CI](https://github.com/Mozcko/miniGameEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/Mozcko/miniGameEngine/actions/workflows/ci.yml)

A small, educational C++17 game engine built on **SDL3 + OpenGL 3.3 Core (via GLEW)**, with a
Minecraft‑style voxel sandbox as its demo game.

The engine owns `main()` and the game loop. A game plugs in by subclassing
`mge::Application` and defining `mge::createApplication()` (the "Hazel‑style" entry‑point
pattern). The current client is `SandboxApp` (`game/SandboxApp.cpp`): it streams in procedurally
generated 16×16×16 block chunks around a free‑flying, collision‑aware camera, animates
water/lava tiles.

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
sudo dnf install cmake gcc-c++ SDL3-devel glew-devel glm-devel stb_image-devel
# (without SDL3-devel, CMake builds SDL3 from source – see docs/BUILDING.md)

scripts/run.sh                   # configure (first time) + build + run, from any directory
# or step by step:
cmake --preset dev && cmake --build --preset dev
./build/sandbox                  # works from any directory (assets are found automatically)
cmake --build --preset dev --target run   # build + run in one CMake command
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
├── CMakeLists.txt          Root build script (options, dependencies, subdirectories)
├── CMakePresets.json       dev / release / ci presets
├── CONTRIBUTING.md         Workflow, conventions, PR rules
├── CLAUDE.md               Condensed context for AI assistants / new contributors
├── README.md               This file
├── LICENSE                 GNU LGPL v2.1
├── docs/                   Cross‑cutting documentation
│   ├── ROADMAP.md          Goals, decisions, milestones → GitHub issues
│   ├── adr/                Architecture decision records
│   ├── ARCHITECTURE.md     Main loop, namespaces, rendering pipeline, ownership
│   ├── BUILDING.md         Dependencies, build/run, troubleshooting
│   └── KNOWN_ISSUES.md     Bugs, quirks and technical debt found while reviewing
├── src/                    Engine source             → src/README.md
│   └── mge/                One folder per CMake target → src/mge/README.md
│       ├── core/           mge_core: headless, no SDL/GPU (Time)
│       ├── platform/       mge_platform: PlatformModule, SDL window + GL context + GLEW, SDL→input
│       ├── render/         mge_render: OpenGL renderer (legacy until #40)
│       │   ├── core/       Vertex, Texture, GLSLProgram, ResourceManager
│       │   ├── 3d/         Mesh (VAO/VBO/EBO)
│       │   ├── 2d/         Sprite, Animation (legacy, unused)
│       │   └── camera/     Camera, Camera2D, Camera3D, FirstPersonCamera
│       ├── audio/          mge_audio: WAV SFX on SDL3 audio streams
│       └── app/            mge_main: main() (core-only)
├── game/                   The game (voxel sandbox)  → game/README.md
├── bench/                  Benchmarks (ecs_bench)
├── cmake/                  CMake helpers: mge_add_game() (MgeGame.cmake)
├── templates/              Game templates for scripts/new-game.sh → templates/README.md
├── examples/               Small engine programs (headless_smoke) → examples/README.md
├── tests/                  doctest unit tests (headless): core/, platform/, game/ → tests/README.md
├── assets/                 Runtime data              → assets/README.md
│   ├── shaders/            GLSL 330 shaders
│   ├── textures/           512×512 block tileset atlas
│   ├── sounds/             WAV sound effects (unused by current game)
│   ├── music/              MP3 music (unused by current game)
│   └── levels/             ASCII 2D level files (legacy, no loader exists)
├── build/, build-*/        CMake build dirs (git-ignored)
└── .vscode/                VS Code: build (Ctrl+Shift+B), run, F5 debug, tests, lint, new game
```

Every folder under `src/`, `game/` and `assets/` has its own `README.md` describing each file in detail.

## Documentation index

| Document | What it covers |
|----------|----------------|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Goals, key decisions, milestones and GitHub issues |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Workflow, branch/commit/PR rules, conventions, labels |
| [docs/adr/](docs/adr/README.md) | Architecture decision records (SDL3 GPU, zero assets, headless core) |
| [CLAUDE.md](CLAUDE.md) | Short "everything you need to know" summary and conventions |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the pieces fit together at runtime |
| [docs/BUILDING.md](docs/BUILDING.md) | Toolchain, dependencies, running, troubleshooting |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | Bugs and debt, with file references |
| [src/README.md](src/README.md) | Source tree overview, include/namespace conventions |
| [src/mge/README.md](src/mge/README.md) | Engine module map |
| [src/mge/core/README.md](src/mge/core/README.md) | `mge_core`: Application runner + modules, Clock/Time, Input, Random, ECS, Log (headless) |
| [src/mge/platform/README.md](src/mge/platform/README.md) | `mge_platform`: PlatformModule, Window, SDL input translation |
| [src/mge/audio/README.md](src/mge/audio/README.md) | `mge_audio`: AudioManager |
| [src/mge/app/README.md](src/mge/app/README.md) | `mge_main`: main() |
| [src/mge/render/README.md](src/mge/render/README.md) | `mge_render` overview |
| [src/mge/render/core/README.md](src/mge/render/core/README.md) | Vertex, Texture, GLSLProgram, ResourceManager |
| [src/mge/render/3d/README.md](src/mge/render/3d/README.md) | Mesh |
| [src/mge/render/2d/README.md](src/mge/render/2d/README.md) | Sprite, Animation (legacy) |
| [src/mge/render/camera/README.md](src/mge/render/camera/README.md) | Camera hierarchy |
| [examples/README.md](examples/README.md) | Example programs (headless smoke test) |
| [game/README.md](game/README.md) | SandboxApp, Chunk, Block (voxel world) |
| [assets/README.md](assets/README.md) | Asset overview and path rules |

## License

GNU Lesser General Public License v2.1 – see [LICENSE](LICENSE).
