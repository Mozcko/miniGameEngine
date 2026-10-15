# 0003. Headless core with optional modules

- Status: Accepted
- Date: 2026-10-14
- Issues: #8, #11, #12, #18

## Context
The engine is currently one executable. `Engine::Application` always creates an SDL window and an
OpenGL context, and game logic is entangled with rendering. For example, constructing a `Chunk`
used to create GPU buffers until #3 separated them. That makes the engine hard to:
- test without a display;
- run as a server or simulation;
- optimize and profile in isolation.

## Decision
- Split the engine into libraries (#8):
  - **`mge_core`**: math, time, logging, ECS, RNG, procedural generation. **No SDL or GPU
    headers.** It builds and runs anywhere.
  - **Optional modules** on top of it: `mge_platform` (SDL3 window, input), `mge_render`
    (SDL3 GPU), `mge_audio`, `mge_voxel`.
- The application runner supports a **headless mode** with no window or GPU, running a fixed
  number of ticks (#11).
- Simulation uses a **fixed timestep** for determinism (#12).
- CI runs headless tests and a headless smoke run on every PR (#3, #18), plus offscreen render
  tests on a software GPU driver (#39).

## Alternatives considered
- **Always‑windowed engine with mocks for tests.** Simpler today, but mocks drift from reality,
  and it rules out servers and simulation tools.
- **Separate "server" build.** Duplicates the build matrix instead of making modules optional.

## Consequences
- Module boundaries are enforced by the build: a core target that includes SDL fails CI.
- New gameplay and procgen code must separate data from presentation.
- Systems such as input must be usable without a device, for example by injecting events (#15).
- Slightly more CMake structure and more targets to maintain.
