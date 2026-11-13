# Changelog

All notable changes to miniGameEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-13

First stable release: everything up to 0.9.0 plus language bindings (M10). Games can now be written in C++, C or Python.

### Added
- **ADR 0006** (#160): language bindings strategy – the C API is the stable ABI, Python uses nanobind over the C++ API.
- **C API v1** (#161): `mge_c` (`<mge/c/mge_c.h>`, C99, opaque handles, `mge_result` + `mge_last_error()`,
  `MGE_C_API_VERSION`), `mge_add_c_game()`, runtime ECS components (`Registry::registerRuntimeComponent`/`addRaw`/`eachRaw`),
  GPU resource tracker (leaked GPU handles are released before the device is destroyed instead of hanging shutdown),
  `examples/c_hello`, `samples/c_consumer`, `docs/C_API.md`.
- **Python bindings v1** (#162): `import mge` via nanobind + scikit-build-core (`pip install .` from the repo root), GIL
  released while the engine runs, Python-defined components, `.pyi` stubs, pytest on Python 3.10/3.12, `docs/PYTHON.md`.
- **Language templates** (#163): `mge new --lang c|python` with `c-3d`, `c-headless`, `python-3d`, `python-2d` templates and a
  language step in the wizard.

### Changed
- The CMake package version file uses semantic versioning (`SameMajorVersion`): any 1.x engine satisfies
  `find_package(mge 1.0)` (0.9.x used `SameMinorVersion`).
- Templates, samples and docs target `1.0` / `v1.0.0`.

## [0.9.0] - 2026-11-11

### Added
- **M0**: Core repository hygiene, CMake presets, doctest unit test harness, and CI workflows (#1–#7).
- **M1**: Headless core foundation: Application lifecycle, Clock, Time, Input, Log, and 3D math/geometry (#8–#18).
- **M1.5**: Modular engine architecture, library decomposition (`mge_core`, modules), and umbrella header (`<mge/mge.h>`) (#96–#100).
- **M2**: Procedural generation (primitives, noise, SDF) and voxel chunk storage, meshing, and raycasting (#19–#27).
- **M3**: SDL3 GPU device abstraction, embedded SPIR-V shader pipeline (`mge_add_shaders`), and 3D mesh rendering (#28–#30, #37–#40).
- **M4**: 2D sprite and quad batching, debug overlays, and font rasterization (#41–#45).
- **M5**: Real-time procedural audio synthesis (oscillators, envelopes, filters, delay, mixer, and music) (#46–#50).
- **M6**: Physics engine: swept AABB voxel collision, spatial hash, rigid bodies, and character controller (#51–#54).
- **M7**: Performance optimizations: job system, chunk streaming, LOD meshing, GPU compute, and profiler scopes (#55–#61).
- **M8**: Showcase applications: voxel sandbox with block editing and audio, SDF glide demo, and engine user guide (#35, #36, #62–#67).
- **M9**: Developer tools and project templates: installable CMake package (`find_package(mge 0.9)` / FetchContent), `mge` CLI (`new`, `build`, `run`, `test`, `check`, `doctor`), interactive project wizard (TUI), and standalone 2D, 3D, and headless project templates (#103, #104, #105, #106).
