# templates/

Standalone game templates used by [`scripts/new-game.sh`](../scripts/new-game.sh) and the `mge new` CLI.

Each template (`3d/`, `2d/`, `headless/`) is a **complete, standalone CMake project** that can be created inside or outside this repository. Each generated project contains:
- `CMakeLists.txt`: looks for an installed engine package via `find_package(mge 0.9 QUIET)` and falls back to `FetchContent` (`GIT_TAG v@MGE_VERSION@`).
- `CMakePresets.json`: `dev` (Debug) and `release` presets matching the engine.
- `.gitignore`: ignores build folders, `.vscode` (except settings), and compiled binaries.
- `.github/workflows/ci.yml`: standalone CI workflow running tests on Ubuntu 24.04.
- `README.md`: build, run, and test instructions.
- `src/`: decoupled headless game logic library (`@GAME_CLASS@Logic.h/.cpp`) and the application entry point (`@GAME_CLASS@.cpp`).
- `tests/`: unit tests (`test_@GAME_NAME@.cpp`) testing game logic with doctest without requiring a window or GPU.

## Available Templates

| Template | Command | Description |
|----------|---------|-------------|
| `3d/` | `scripts/new-game.sh <name> --template 3d` | Procedural 3D scene with ground plane and primitive meshes (`primitives`), procedural sky with `DayNight` cycle and directional lighting, a `CharacterController` on a flat `SolidQuery` floor, ECS entity moving in the Fixed phase, F1 `DebugOverlay` with profiler, and synthesized jump sound (`sfx::jump`). Smoke test: 60 frames offscreen. |
| `2d/` | `scripts/new-game.sh <name> --template 2d` | 2D pixel-space scene with `Renderer2D`, procedural shapes, built-in font text, player square moving in the Fixed phase with rectangle collision, collectible coins with procedural sound (`sfx::coin`), and score HUD. Smoke test: 60 frames offscreen. |
| `headless/` | `scripts/new-game.sh <name> --headless` | Deterministic simulation with `ManualClock` (fixed 60 Hz steps) and no modules (no window, GPU, or SDL). Verifies simulation determinism on shutdown and exits 0/1. Smoke test: 60 frames. |
| `window/` | `scripts/new-game.sh <name> --template window` | Legacy single-file example: pulsing clear colour, ECS pulse component, F1 performance overlay. Smoke test: 30 frames offscreen. |

## Placeholders

Files inside templates use the `.in` suffix and contain placeholders substituted by `scripts/new-game.sh`:

| Placeholder | Example for `space_shooter` | Description |
|-------------|-----------------------------|-------------|
| `@GAME_NAME@` | `space_shooter` | Lowercase snake_case identifier (CMake target, test executable, log category). |
| `@GAME_CLASS@` | `SpaceShooter` | PascalCase class name and C++ source filename. |
| `@GAME_TITLE@` | `Space Shooter` | Human-readable title for window headers and documentation. |
| `@MGE_VERSION@` | `0.9.0` | Target engine release version for FetchContent fallbacks. |

The `.in` suffix keeps template source files out of clang-format and clang-tidy in-tree, as placeholders are not valid C++ identifiers. When a project is generated, `scripts/new-game.sh` runs `clang-format` on all generated `.h` and `.cpp` files.

## How to Add a New Template

To add a new project template `templates/<name>/`:

1. **Create the template directory**:
   ```bash
   mkdir -p templates/<name>/src templates/<name>/tests templates/<name>/.github/workflows
   ```

2. **Add project configuration**:
   - `templates/<name>/CMakeLists.txt.in`: configure `find_package(mge 0.9 QUIET)` with `FetchContent` fallback, an isolated static library `@GAME_NAME@_logic` for headless game logic, `mge_add_game(@GAME_NAME@ SOURCES src/@GAME_CLASS@.cpp LIBRARIES @GAME_NAME@_logic ...)` with `SMOKE_FRAMES 60`, and a doctest test target `@GAME_NAME@_tests`.
   - `templates/<name>/CMakePresets.json`: provide `dev` and `release` configure, build, and test presets.
   - `templates/<name>/.gitignore`: ignore `build*/`, `.vscode/*` (except `settings.json`), and `*.spv`.
   - `templates/<name>/.github/workflows/ci.yml`: setup CI on `ubuntu-24.04` installing dependencies and running `ctest --preset dev`.
   - `templates/<name>/README.md.in`: document building and running commands.

3. **Add C++ source templates**:
   - `src/@GAME_CLASS@Logic.h.in` and `src/@GAME_CLASS@Logic.cpp.in`: pure, testable game state and functions (code comments in Spanish).
   - `src/@GAME_CLASS@.cpp.in`: `mge::Application` subclass connecting modules, input actions, rendering, and ECS systems.
   - `tests/test_@GAME_NAME@.cpp.in`: doctest unit tests covering logic without GPU/window dependencies.

4. **Test the template**:
   ```bash
   scripts/new-game.sh test_template --template <name> --dir /tmp/test_template
   cd /tmp/test_template
   cmake --preset dev
   cmake --build --preset dev
   ctest --preset dev
   ```

5. **Register in CI**:
   Add a verification step to the `templates` job in `.github/workflows/ci.yml` so the new template is continuously tested.
