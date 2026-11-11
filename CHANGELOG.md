# Changelog

All notable changes to miniGameEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Reusable engine packaging: installable CMake package (`find_package(mge 0.9)`) and FetchContent support (#103).

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
