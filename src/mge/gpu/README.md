# src/mge/gpu/  →  target `mge_gpu`

**Low‑level GPU access on the SDL3 GPU API** (Vulkan today; Metal and D3D12 later), per
[ADR 0001](../../../docs/adr/0001-sdl3-gpu-api.md):
- move‑only RAII wrappers;
- `GpuDevice` (device and resource factory, uploads, readback);
- `CommandBuffer` with scoped render and copy passes;
- `GpuModule`, which owns the device and the window's swapchain.

It depends on `mge_core`, `mge_procgen` (for `MeshData`), `mge_platform` (the window) and SDL3.
Programs that link it see `MGE_HAS_GPU`, and `<mge/mge.h>` then includes it.

The high‑level renderer (pipelines, cameras, materials) arrives in `mge_graphics` (#30). The legacy
OpenGL renderer (`mge_render`) is deleted in #40. Shaders are compiled at build time and embedded (#29).

| File | Contents |
|------|----------|
| `Handles.h` | `GpuHandle<T, Release>` and the aliases `GpuBuffer`, `GpuTransferBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuGraphicsPipeline`, `GpuComputePipeline` |
| `GpuDevice.h/.cpp` | `GpuDeviceOptions`, `GpuDevice` |
| `CommandBuffer.h/.cpp` | `CommandBuffer`, `RenderPass`, `CopyPass`, `ColorTarget`, `DepthTarget` |
| `GpuMesh.h/.cpp` | `GpuVertex` (48 B), `interleave(MeshData)`, vertex layout for pipelines, `GpuMesh` |
| `GpuModule.h/.cpp` | `GpuModule`, `GpuFrame` |

## Using it in a game

```cpp
MyGame() : mge::Application(config) {
    mge::PlatformOptions platform;
    platform.openGl = false;                 // the window belongs to SDL GPU, not to an OpenGL context
    addModule<mge::PlatformModule>(platform);
    addModule<mge::GpuModule>();             // AFTER the platform: it needs the window
}

void onRender() override {
    const mge::GpuFrame* frame = getModule<mge::GpuModule>()->frame();
    if (!frame) return;                      // minimised window: no image this frame
    mge::ColorTarget target;
    target.texture = frame->swapchain;
    target.clearColor = {0.1f, 0.2f, 0.3f, 1.0f};
    mge::RenderPass pass = frame->cmd->beginRenderPass(target);   // ends at scope exit
    // ... draw calls (pipelines come with #29/#30)
}
```

**`GpuModule` lifecycle:**
1. `onInit` needs a `PlatformModule` without a GL context, added before it. It creates the device
   and claims the window.
2. `onFrameBegin` runs after the platform has polled events. It acquires a command buffer and the
   swapchain image.
3. The game records into `frame()->cmd` in `onUpdate`/`onRender`.
4. `onFrameEnd` submits.
5. `onShutdown` waits for the GPU, releases the window, then destroys the device. Shutdown runs in
   reverse order, so this happens before the platform destroys the window.

`frame()->width/height` is the real swapchain size, which follows window resizes.

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

## Tests and examples
- **`tests/gpu/` → `mge_gpu_tests`** (label `gpu`): clear and read back, mesh upload, move
  semantics, cancelled command buffers, and no validation messages.
  - They run **without a window** (SDL `offscreen` video driver, rendering into textures).
  - With no compatible GPU they're skipped with a message. `MGE_REQUIRE_GPU=1`, set in CI, turns
    that into a failure.
  - CI uses Mesa **lavapipe** (software Vulkan). Locally the real GPU is used; to try lavapipe:
    `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`.
- **`examples/gpu_clear`:** a window whose clear colour pulses. Its smoke test runs 60 frames
  offscreen and fails if no frame was presented.
