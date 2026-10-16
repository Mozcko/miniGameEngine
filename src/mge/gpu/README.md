# src/mge/gpu/  →  target `mge_gpu`

**Low‑level GPU access on the SDL3 GPU API** (Vulkan today; Metal and D3D12 later), per
[ADR 0001](../../../docs/adr/0001-sdl3-gpu-api.md):
- move‑only RAII wrappers;
- `GpuDevice` (device and resource factory, uploads, readback);
- `CommandBuffer` with scoped render and copy passes;
- `GpuModule`, which owns the device and the window's swapchain.

It depends on `mge_core`, `mge_procgen` (for `MeshData`), `mge_platform` (the window) and SDL3.
Programs that link it see `MGE_HAS_GPU`, and `<mge/mge.h>` then includes it.

The high‑level renderer (textures, mesh drawing) is [`mge_graphics`](../graphics/README.md).

| File | Contents |
|------|----------|
| `Shader.h` | `ShaderCode` (embedded SPIR‑V + stage), `ShaderResources` (resource counts) and the binding convention |
| `Pipeline.h` | `GraphicsPipelineDesc` (shaders, colour/depth formats, `GpuVertex` input or none, cull, blend) |
| `Handles.h` | `GpuHandle<T, Release>` and the aliases `GpuBuffer`, `GpuTransferBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuGraphicsPipeline`, `GpuComputePipeline` |
| `GpuDevice.h/.cpp` | `GpuDeviceOptions`, `GpuDevice` |
| `CommandBuffer.h/.cpp` | `CommandBuffer`, `RenderPass`, `CopyPass`, `ColorTarget`, `DepthTarget` |
| `GpuMesh.h/.cpp` | `GpuVertex` (48 B), `interleave(MeshData)`, vertex layout for pipelines, `GpuMesh` |
| `GpuModule.h/.cpp` | `GpuModule`, `GpuFrame` |
| `Swapchain.h/.cpp` | `choosePresentMode`, `chooseDepthFormat`, `presentModeName` (pure selection logic, tested without a GPU) |

## Using it in a game

```cpp
MyGame() : mge::Application(config) {
    addModule<mge::PlatformModule>();        // the window
    addModule<mge::GpuModule>();             // AFTER the platform: it needs the window
}

void onRender() override {
    const mge::GpuFrame* frame = getModule<mge::GpuModule>()->frame();
    if (!frame) return;                      // minimised window: no image this frame
    mge::ColorTarget target;
    target.texture = frame->swapchain;
    target.clearColor = {0.1f, 0.2f, 0.3f, 1.0f};
    mge::RenderPass pass = frame->cmd->beginRenderPass(target);   // ends at scope exit
    SDL_BindGPUGraphicsPipeline(pass.get(), m_pipeline.get());
    SDL_DrawGPUPrimitives(pass.get(), 3, 1, 0, 0);
}
```

**`GpuModule` lifecycle:**
1. `onInit` needs a `PlatformModule`, added before it. It creates the device
   and claims the window.
2. `onFrameBegin` runs after the platform has polled events. It acquires a command buffer and the
   swapchain image.
3. The game records into `frame()->cmd` in `onUpdate`/`onRender`.
4. `onFrameEnd` submits.
5. `onShutdown` waits for the GPU, releases the window, then destroys the device. Shutdown runs in
   reverse order, so this happens before the platform destroys the window.

`frame()->width/height` is the real swapchain size, which follows window resizes.

## Presentation, depth and device info (#37)
- **Vsync:**
  - `ApplicationConfig::vsync` (default true, `MGE_VSYNC=0|1` overrides it) picks the present
    mode at init.
  - **On:** VSYNC (no tearing, capped at the monitor's Hz).
  - **Off:** MAILBOX if the window supports it, else IMMEDIATE, else VSYNC with a warning (`choosePresentMode`).
  - `GpuModule::setVsync(bool)` switches at runtime; `gpu_triangle` toggles it with **V** and logs FPS.
- **Depth:** `GpuDevice::depthFormat()` is chosen once: D32_FLOAT, else D24_UNORM, else D16
  (`chooseDepthFormat`). `createDepthTarget(w, h)` creates a depth buffer, passed to
  `beginRenderPass(color, &depth)`.
- **Startup log:** the GPU name, driver, version and depth format, e.g.
  `Device vulkan: Intel(R) Iris(R) Xe Graphics (… Mesa 26.2.3), profundidad D32`.
- **Validation:** `MGE_GPU_DEBUG=0|1` overrides `GpuDeviceOptions::debug` (on by default in Debug builds).

## Shaders and pipelines (#29)

GLSL is compiled to SPIR‑V at build time and embedded with `mge_add_shaders(target SHADERS a.vert
b.frag)` (see `docs/BUILDING.md` → Shaders). Each shader becomes `mge::shaders::k<Name><Stage>`, so
no files are read at runtime.

```cpp
#include <shaders/triangle_vert.h>
#include <shaders/triangle_frag.h>

mge::ShaderResources vsResources;
vsResources.uniformBuffers = 1;                      // uniform block at set 1, binding 0
mge::GpuShader vs = device.createShader(mge::shaders::kTriangleVert, vsResources);
mge::GpuShader fs = device.createShader(mge::shaders::kTriangleFrag);

mge::GraphicsPipelineDesc desc;
desc.vertexShader = &vs;
desc.fragmentShader = &fs;
desc.colorFormat = gpuModule->swapchainFormat();
desc.gpuVertexInput = false;                          // true (default): GpuVertex buffers in slot 0
m_pipeline = device.createGraphicsPipeline(desc);     // the shaders can be released afterwards
```

**`GraphicsPipelineDesc` fields:**
- `depthFormat`: `INVALID` means no depth buffer. Also `depthTest` and `depthWrite`.
- `primitive`, `cullMode` (back by default) and `frontFace` (counter‑clockwise).
- `blend`: `None`, `Alpha` or `Additive`.

Uniforms are pushed per draw with `SDL_PushGPUVertexUniformData` / `SDL_PushGPUFragmentUniformData`
(slot = binding). The **binding convention** is in `Shader.h`:

| Stage | Textures and buffers | Uniform buffers |
|-------|----------------------|-----------------|
| vertex | set 0 | set 1 |
| fragment | set 2 | set 3 |

`ShaderResources` must declare how many of each a shader uses. SDL doesn't read them from the
SPIR‑V, so a mismatch is reported by the validation layers.

## Resources and ownership
- **Handles are move‑only.** Copying a `GpuBuffer` doesn't compile, and moving it transfers
  ownership. `reset()` releases a handle early.
- **Release order.** SDL waits until the GPU has finished with a resource before freeing it. Every
  handle must still be released **before its `GpuDevice`**; the module and the game's members
  ensure this.
- **`GpuDevice::create(options)`:**
  - requires SDL's video subsystem (Vulkan is loaded through it);
  - returns `nullptr` when there's no compatible GPU, with the reason in the log;
  - `options.debug` defaults to on in Debug builds. It enables the validation layers when they
    are installed (`vulkan-validation-layers` on Fedora, `vulkan-validationlayers` on Ubuntu).
- **Factories:** `createBuffer`, `createTransferBuffer`, `createTexture`, `createRenderTarget`
  (2D RGBA8 colour target that can also be sampled), `createSampler`, `createShader`,
  `createGraphicsPipeline`, `createComputePipeline`.
- **`uploadMesh(MeshData)`:**
  - interleaves the mesh into `GpuVertex {position, normal, color, uv}`. Shader locations are
    0, 1, 2, 3; colour is included, unlike the GL path (KI#13).
  - uploads vertices and `uint32` indices through transfer buffers on its own command buffer,
    without waiting;
  - an empty mesh gives an empty `GpuMesh`.
- **`downloadTexture(texture, w, h)`:** reads RGBA8 back to the CPU. It blocks, and is meant for
  tests and screenshots.
- **`CommandBuffer`:**
  - `submit()`, or `submitAndWait()` (fence);
  - destroyed without submitting, it is cancelled. When a swapchain image was acquired, SDL
    doesn't allow cancelling, so it's submitted empty.
  - `beginRenderPass(color, depth*)` and `beginCopyPass()` return passes that end at scope exit.

## Coordinate conventions
SDL GPU uses the same conventions on every backend, converting for Vulkan internally:
- **NDC:** Y up, Z in [0, 1].
- **Viewport and texture coordinates:** origin at the top‑left.

So use `glm::perspectiveRH_ZO` and the usual `glm::lookAt`. MeshData's counter‑clockwise winding
is then front‑facing, matching the pipeline default.

## Tests and examples
- **`tests/gpu/` → `mge_gpu_tests`** (label `gpu`): clear and read back, mesh upload, move
  semantics, cancelled command buffers, embedded SPIR‑V (magic number, stage, alignment), and a
  triangle with a fragment uniform drawn and read back.
- **Validation in CI.** The tests request the Vulkan validation layers, which print errors to the
  test's output. Every GPU test and GPU smoke test has the CTest property
  `FAIL_REGULAR_EXPRESSION "Validation Error"`, so API misuse, or a resource still alive when the
  device is destroyed, fails the test.
  - `MGE_REQUIRE_VALIDATION=1` (CI) fails if the layers aren't active.
  - `gpu_validation_probe` triggers a real error on purpose and passes only if its VUID appears in
    the output, which proves the mechanism works.
  - Locally, without the layers installed, the probe is skipped.
  - They run **without a window** (SDL `offscreen` video driver, rendering into textures).
  - With no compatible GPU they're skipped with a message. `MGE_REQUIRE_GPU=1`, set in CI, turns
    that into a failure.
  - CI uses Mesa **lavapipe** (software Vulkan). Locally the real GPU is used; to try lavapipe:
    `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`.
- **`tests/render/` → `render_tests`:** golden‑image scenes (see `tests/README.md`).
- **`examples/gpu_triangle`:** a rotating triangle (embedded shaders, a vertex uniform) on a
  pulsing background. Its smoke test runs 60 frames offscreen and fails if no frame was presented.
