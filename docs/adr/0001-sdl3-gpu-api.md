# 0001. Use SDL3 and the SDL3 GPU API instead of OpenGL

- Status: Accepted
- Date: 2026-10-14
- Issues: #9, #28, #29, #30, #40

## Context
The engine renders through OpenGL 3.3 core with GLEW, and uses SDL2 for the window and input. The
project aims to be heavily optimized and commercially viable long term.
- OpenGL has had no new versions since 4.6 (2017).
- macOS has deprecated OpenGL.
- OpenGL's implicit global state makes multithreaded command submission and predictable
  performance hard.
- SDL2 is in maintenance mode; SDL3 is the supported line.

Rendering in this engine is procedural (generated meshes, shader‑based materials, SDF
raymarching). So it benefits from compute shaders, explicit control over GPU work, and shaders
embedded as bytecode, which also fits [ADR 0002](0002-zero-asset-policy.md).

## Decision
- Migrate the platform layer from SDL2 to **SDL3** (#9).
- Render through the **SDL3 GPU API**. It runs on **Vulkan** (Linux, Windows, Android), **Metal**
  (macOS, iOS) and **Direct3D 12** (Windows, Xbox).
- Compile shaders at build time to SPIR‑V first, adding MSL and DXIL as platforms are added, and
  embed the bytecode in the executable (#29).
- Build **no new features on OpenGL**. Port the existing renderer to parity (#30), then delete
  OpenGL and GLEW (#40).

## Alternatives considered
- **Stay on OpenGL.** Simplest, but a dead end for long‑term support, macOS and modern
  performance features.
- **Raw Vulkan behind an engine RHI.** Maximum control, but several times more code: setup,
  memory, synchronization. It also needs separate Metal and D3D12 backends for full platform reach.
  Too costly for a small team at this stage.
- **bgfx / wgpu / Diligent.** Capable, but each adds a large third‑party dependency and its own
  abstractions. SDL is already a dependency, and SDL3's GPU API covers our needs.

## Consequences
- The engine gets modern backends on every major platform with a thin wrapper layer (#28).
- Shaders need a build‑time toolchain (SDL_shadercross or glslc + SPIRV‑Cross), plus a resource
  binding convention.
- GPU tests in CI need a software Vulkan driver: Mesa lavapipe (#39).
- Until #40, two renderers coexist. Only the SDL GPU path gets new work.
- If SDL GPU ever becomes limiting, the wrappers from #28 are the seam where a raw Vulkan backend
  could be added.
