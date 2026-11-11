# Changelog

All notable changes to miniGameEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-12

First stable release: the engine MVP (M0–M8) plus the developer workflow of M9.

### Added
- **Engine as a package** (#103): installable CMake package (`-DMGE_INSTALL=ON`, `find_package(mge 1.0)`) exporting
  `mge::core`, `procgen`, `voxel`, `physics`, `synth`, `main`, `platform`, `gpu`, `graphics`, `audio`, with `mge_add_game` and
  `mge_add_shaders`; FetchContent support (`GIT_TAG v1.0.0`); `<mge/Version.h>`; out-of-tree sample in `samples/consumer`.
- **Project templates** (#106): standalone `3d`, `2d` and `headless` templates with presets, a doctest target, README,
  `.gitignore` and a CI workflow; CI generates, builds and tests every template against the installed engine.
- **`mge` CLI** (#104): `new`, `build`, `run`, `test`, `check [--fix]`, `clean`, `doctor`, `--version`; project detection via
  `mge.project`; child processes started without a shell (Linux/macOS; not built on Windows yet).
- **`mge new` wizard** (#105): interactive TUI (name, template, audio, license, git, VS Code) with a flag for every question,
  non-interactive fallback, clean cancel on Ctrl+C/EOF; ADR 0005.

### Changed
- `MGE_INSTALL` is opt-in (default `OFF`); the source-tree asset fallback stays `ON` for dev builds (warning when combined
  with `MGE_INSTALL`).
- The package version file uses semantic versioning (`SameMajorVersion`): any 1.x engine satisfies `find_package(mge 1.0)`.
- `scripts/new-game.sh` is now a thin wrapper around `mge new`.

## [0.9.0] - 2026-11-10

### Added
- **M0**: Core repository hygiene, CMake presets, doctest unit test harness, and CI workflows.
- **M1**: Headless core foundation: Application lifecycle, Clock, Time, Input, Log, and 3D math/geometry.
- **M1.5**: Modular engine architecture, library decomposition (`mge_core`, modules), and umbrella header (`<mge/mge.h>`).
- **M2**: Procedural generation (primitives, noise, SDF) and voxel chunk storage, meshing, and raycasting.
- **M3**: SDL3 GPU device abstraction, embedded SPIR-V shader pipeline (`mge_add_shaders`), and 3D mesh rendering.
- **M4**: 2D sprite and quad batching, debug overlays, and font rasterization.
- **M5**: Real-time procedural audio synthesis (oscillators, envelopes, filters, delay, mixer, and music).
- **M6**: Physics engine: swept AABB voxel collision, spatial hash, rigid bodies, and character controller.
- **M7**: Performance optimizations: job system, chunk streaming, LOD meshing, GPU compute, and profiler scopes.
- **M8**: Showcase applications: voxel sandbox with block editing and audio, SDF glide demo, and engine user guide.
