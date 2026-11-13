# Python Bindings for miniGameEngine (`mge`)

This directory contains the nanobind-based Python bindings for `miniGameEngine`, providing full access to the engine's core, ECS, math, rendering, procedural audio, and voxel subsystems.

See [docs/PYTHON.md](../../docs/PYTHON.md) for the comprehensive guide and API reference.

## Directory Structure

- `CMakeLists.txt`: CMake build configuration for the `_mge` extension module.
- `mge/`: The Python package directory containing:
  - `__init__.py`: Package initialization and exports.
  - `__init__.pyi`: Type stub re-exports.
  - `_mge.pyi`: Generated nanobind type stub interface.
  - `py.typed`: PEP 561 marker indicating typed package.
- `src/`: C++ nanobind binding sources:
  - `Common.h`: Shared includes and type definitions.
  - `App.h` / `App.cpp`: `Application` trampoline, `InputState`, `Time`.
  - `Math.cpp`: `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat4`, buffer protocol implementation.
  - `Ecs.h` / `Ecs.cpp`: Sparse-set ECS integration, Python custom component bridge.
  - `Render.h` / `Render.cpp`: `Mesh`, `Texture2D`, `Lighting`, `Renderer`.
  - `Audio.h` / `Audio.cpp`: `Sample`, `Audio`, procedural SFX bindings.
  - `Voxel.cpp`: `BlockDesc`, `BlockRegistry`, `VoxelWorld`, procedural generator, raycasting.
  - `Module.cpp`: Nanobind module definition and exceptions.
- `tests/`: Complete pytest test suite:
  - `test_lifecycle.py`: Engine lifecycle and loop tests.
  - `test_math.py`: Vector and matrix math, buffer protocol, NumPy interop.
  - `test_ecs.py`: ECS components, queries, weakref safety, systems.
  - `test_input.py`: Actions, axes, and input injection.
  - `test_audio.py`: Procedural sound synthesis and mixer controls.
  - `test_voxel.py`: Voxel terrain, chunk loading, DDA raycasting.
  - `test_gpu_safety.py`: Handle invalidation and safety post-shutdown.
  - `test_gil.py`: Concurrency and GIL release verification.

## Building & Testing

```bash
# Build C++ extension
cmake -B build -G Ninja -DMGE_BUILD_PYTHON=ON
cmake --build build --target _mge

# Run test suite
PYTHONPATH=bindings/python pytest bindings/python/tests

# Update type stubs
PYTHONPATH=bindings/python python3 -m nanobind.stubgen -m mge._mge -O bindings/python/mge
```
