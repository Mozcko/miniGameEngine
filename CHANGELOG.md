# Changelog

All notable changes to miniGameEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **LOD terrain column height** (#179): `buildLodChunkMesh` now emits exposed top faces at the exact height (`highest opaque y + 1`) of each $f\times f$ column instead of the coarse $f^3$ cell top, and adjusts side faces to span up to `y_top` and bridge height steps between adjacent columns while preserving greedy merging and border skirts.
- **Stable runtime ECS component storage** (#176): `ecs_detail::RawPool` now uses fixed 64-element aligned pages instead of a single reallocating buffer, so pointers from `addRaw`/`getRaw`/`eachRaw` (`mge_component_add`/`get`/`each`) remain valid across adds (the spawner pattern). Removal (`removeRaw`/`destroy`) keeps swap-and-pop and invalidates pointers to that component and to the one that was last in its pool.

## [1.0.1] - 2026-11-18

Bug-fix release: the M10 review fixes (#174) and #177, which landed after the `v1.0.0` tag.

### Fixed
- **Runtime ECS components** (#174): registering an existing name with a different size/alignment is now an error
  (`kInvalidComponentType` / `MGE_INVALID_COMPONENT_ID` + `mge_last_error()` / `mge.EngineError`) instead of silently
  reusing the old pool (heap overflow); alignment must be a power of two ≤ 4096, size < 1 MiB.
- **Release-safe bounds checks** (#174): `Registry::addRaw/getRaw` and every C function taking a component id validate it in
  all build types (an invalid id returned out-of-bounds memory in Release).
- **Iteration safety** (#174): `Registry::eachRaw`, `View::each` and `mge_component_each` iterate a copy of the entity list and
  re-check membership, so destroying or adding entities inside the callback no longer skips entities or touches freed memory;
  pointer lifetimes are documented in `mge_c.h` and `docs/C_API.md`.
- **C API ABI** (#174): `struct_size` of `mge_app_config` / `mge_app_callbacks` is enforced (too small → error; larger structs
  from newer headers are accepted).
- **Python components** (#174): keyed by `module.qualname`, so classes with the same `__name__` in different modules no longer
  share a pool.
- **Templates** (#174): `mge new --lang c|python` refuses engine versions older than the one that introduced the language.
- **sdf_glide mouse steering** (#177, #191, thanks @Golden007-prog): mouse motion is accumulated between fixed steps, so
  steering no longer gets weaker above 60 FPS (and is no longer applied twice below 60 FPS).

### Added
- Python voxel generators can fill a whole chunk at once (`generate_chunk(cx, cy, cz)` → 4096 block ids) instead of one call
  per block (#174).

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
