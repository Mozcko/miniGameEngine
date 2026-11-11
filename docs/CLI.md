# `mge` CLI

`mge` is a cargo-like developer command-line tool for `miniGameEngine`. It provides unified commands to initialize, build, run, test, format, and diagnose projects created with the engine.

## Installation and Availability

When the engine is compiled as a top-level project (`MGE_BUILD_CLI=ON`), the `mge` executable is built directly into the root of the build directory (`build/mge`).

When the engine is installed (`MGE_INSTALL=ON`), the tool is installed to `bin/mge` and its project templates are placed in `share/mge/templates/`.

```bash
# Check version
mge --version

# Verify development environment tools
mge doctor
```

## Subcommands

### `mge new`

Scaffolds a new standalone or in-tree game project from an official engine template.

```bash
mge new <name> [--template <3d|2d|headless>] [--dir <path>] [--no-git]
```

- `<name>`: Project identifier. Must match `^[a-z][a-z0-9_]*$` and cannot use engine reserved names (`sandbox`, `run`, `mge_core`, etc.).
- `--template <tmpl>`: Template type:
  - `3d` (default): 3D procedural world with camera controller, lighting, sky, character controller, and sound.
  - `2d`: Pixel-space 2D game with `Renderer2D`, procedural shapes, bitmap font, collision, and sound.
  - `headless`: Offscreen deterministic simulation without SDL or GPU dependencies.
- `--dir <path>`: Target directory for project files (defaults to `./<name>`).
- `--no-git`: Disables automatic `git init` in the created project directory.

`mge new` generates an `mge.project` file in the project root:

```ini
name=my_game
template=3d
```

### `mge build`

Configures and builds the current project or engine target using CMake presets.

```bash
mge build [--release] [--preset <preset>]
```

- `--release`: Compiles using the `release` preset (equivalent to `--preset release`).
- `--preset <p>`: Selects a specific CMake preset (defaults to `dev`).

### `mge run`

Builds the target if necessary, and immediately executes the game binary.

```bash
mge run [--release] [--preset <preset>] [-- <game_args...>]
```

- Runtime options after `--` are passed directly to the game binary.
- Example:
  ```bash
  mge run -- --smoke 60
  ```

### `mge test`

Executes the project's test suite via CTest presets.

```bash
mge test [--release] [--preset <preset>]
```

- Defaults to the `dev` test preset.

### `mge check`

Validates code format and guidelines (checks by default, exits 1 on errors; pass `--fix` to apply corrections).

```bash
mge check [--fix]
```

- Inside the engine repository, delegates to `scripts/lint.sh`.
- Inside generated game projects, runs `clang-format` (`--dry-run --Werror` or `-i` with `--fix`) across `src/` and `tests/`.

### `mge clean`

Removes generated build directories (`build`, `build-*`) in the detected project root.

```bash
mge clean
```

### `mge doctor`

Audits your system environment and dependencies required to build and run games with `miniGameEngine`:

1. **C++ Compiler**: Verifies GCC >= 11 or Clang >= 14 with full C++17 support.
2. **CMake**: Checks for CMake >= 3.24.
3. **Ninja**: Optional build tool check.
4. **SDL3**: Checks system installation via `pkg-config` or standard headers.
5. **glslang**: Verifies `glslang` or `glslangValidator` GLSL-to-SPIRV compiler.
6. **Vulkan Loader & ICD**: Tests `vulkaninfo --summary`, loader libraries, and driver ICD manifests (`/usr/share/vulkan/icd.d`).
7. **GLM Headers**: Checks for `<glm/glm.hpp>` header files.

Outputs clear `[✓]`, `[✗]`, or `[!]` indicators with installation suggestions on failure. Exits with return code `0` on success or `1` if any required dependency is missing.

## Project Detection

Commands such as `build`, `run`, `test`, `check`, and `clean` automatically detect the project root by searching upwards from the current working directory for:
1. An `mge.project` file (identifies a game project).
2. A `CMakeLists.txt` file containing `project(miniGameEngine` (identifies the engine repository root).

## Template Lookup Resolution

`mge` locates project templates using the following priority order:
1. Environment variable `$MGE_TEMPLATES_DIR` (if set).
2. Relative share directory `<exe_dir>/../share/mge/templates` (standard installation layout).
3. Compile-time development tree `MGE_SOURCE_TEMPLATES_DIR`.
4. Working directory fallback `./templates` or `../templates`.
