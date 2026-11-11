# mge CLI Tool

The `mge` CLI tool is a cargo-like developer utility for `miniGameEngine`. It provides commands to create, build, run, test, format, and check projects built with the engine.

## Subcommands

- `mge new <name> [--template 3d|2d|headless] [--dir <path>] [--no-git]`: Scaffolds a new standalone or in-tree game project from standard templates.
- `mge build [--release] [--preset <p>]`: Configures and compiles the project using CMake presets.
- `mge run [--release] [--preset <p>] [-- <args...>]`: Builds and executes the game binary, passing optional runtime arguments.
- `mge test [--release] [--preset <p>]`: Runs project unit tests via CTest.
- `mge check [--fix]`: Checks source file formatting (exits 1 on errors; pass `--fix` to format in place with `clang-format`).
- `mge clean`: Deletes generated build directories.
- `mge doctor`: Verifies development environment prerequisites and tools.

## Architecture

- `Process.h` / `Process.cpp`: Fork/execvp-based process runner preventing shell injection.
- `Colors.h`: ANSI terminal colors with automatic TTY and `NO_COLOR` detection.
- `Project.h` / `Project.cpp`: Project metadata and detection, template interpolation, name validation.
- `Doctor.h` / `Doctor.cpp`: Diagnostics checks for C++17 compiler, CMake, Ninja, SDL3, glslang, Vulkan loader + ICD, GLM.
- `Commands.h` / `Commands.cpp`: Subcommand implementations.
- `Main.cpp`: CLI entry point.
