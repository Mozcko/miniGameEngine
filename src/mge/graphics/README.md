# src/mge/graphics/  →  target `mge_graphics`

**The high‑level renderer on SDL3 GPU** (see [`mge_gpu`](../gpu/README.md)): textures and mesh
drawing with a camera, depth and transparency. It depends on `mge_gpu` and `mge_procgen`. Programs
that link it see `MGE_HAS_GRAPHICS`, and `<mge/mge.h>` then includes it. `mge_add_game` links it
by default (`MODULES platform gpu graphics audio`).

| File | Contents |
|------|----------|
| `Texture2D.h/.cpp` | `Texture2D`: RGBA8 texture from a file (stb_image) or from pixels, region updates, tile copies from the original image |
| `MeshRenderer.h/.cpp` | `MeshRenderer`, `MeshPass`: draws `GpuMesh`es with texture × vertex colour, camera, depth |
| `shaders/mesh.vert/.frag` | Embedded GLSL (`mge_add_shaders`): `viewProjection * model * position`, `texture(albedo, uv) * color` |

Coming in M3: the render queue with a cutout pass (#31), lighting
and fog (#33), procedural materials (#34) and debug drawing (#38).

## `Texture2D`

```cpp
mge::GpuDevice& device = getModule<mge::GpuModule>()->device();
mge::Texture2D atlas = mge::Texture2D::fromFile(device, mge::assetPath("textures/atlas.png"));   // asset path
mge::Texture2D noise = mge::Texture2D::fromPixels(device, 64, 64, pixels.data());               // code path
```

Both paths are supported ([ADR 0004](../../../docs/adr/0004-procedural-first-assets-optional.md)).
Pixels are RGBA8 with **row 0 at the top** (image order).

**UV convention** (the engine's, unchanged from the OpenGL renderer):
- UV (0, 0) is the image's **bottom‑left** and v grows upwards. MeshData, the primitives and
  `mge_voxel`'s `TextureAtlas` all use it.
- SDL GPU's texture origin is the top‑left, so `Texture2D` stores the image flipped vertically.
  Callers never see this: the pixel coordinates in this API are image coordinates.

| Member | Notes |
|--------|-------|
| `fromFile(device, path, keepCpuCopy)` | Any format stb_image reads, converted to RGBA. Invalid (`!texture`) on failure, logged |
| `fromPixels(device, w, h, rgba, keepCpuCopy)` | Generated textures |
| `white(device)` | 1×1 white (used for untextured meshes) |
| `updateRegion(device, x, y, w, h, rgba)` | Replace a rectangle (image coordinates). False if out of bounds |
| `copyFromOriginal(device, srcX, srcY, w, h, dstX, dstY)` | Copy a region of the **original** image (needs `keepCpuCopy`) somewhere else, e.g. animated atlas tiles |
| `width()`, `height()`, `gpu()`, `originalPixels()` | |

stb_image is compiled here with `STB_IMAGE_STATIC`, so it doesn't clash with the legacy GL
renderer's copy until #40 removes that.

## `MeshRenderer`

```cpp
m_renderer = std::make_unique<mge::MeshRenderer>(device, gpuModule->swapchainFormat());
mge::GpuMesh mesh = device.uploadMesh(mge::primitives::box(glm::vec3(1.0f)));

void onRender() override {
    const mge::GpuFrame* frame = gpuModule->frame();
    if (!frame || !m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height, sky)) return;
    m_renderer->setCamera(glm::perspectiveRH_ZO(fov, aspect, 0.1f, 500.0f) * view);   // Z in [0, 1]
    m_renderer->draw(mesh, model, &atlas);                          // opaque (default)
    m_renderer->draw(water, model, &atlas, mge::MeshPass::Transparent);   // after the opaque ones
    m_renderer->end();
}
```

| Feature | Behaviour |
|---------|-----------|
| **Passes** | `Opaque`: depth test + write, no blending. `Transparent`: alpha blending, depth test **without** write. Draw opaque first. Sorting comes with the render queue (#31) |
| **Culling** | Back faces culled, counter‑clockwise = front (MeshData's winding) |
| **Shading** | `texture(albedo, uv) * vertexColor`. No texture → 1×1 white, so only the vertex colour shows (colour‑only meshes, AO) |
| **Sampler** | Nearest filtering, clamp to edge (pixel‑art look, as the GL renderer) |
| **Depth** | Owned by the renderer, `device.depthFormat()` (D32/D24), recreated when the target size changes (window resize) |
| **Uniforms** | Vertex set 1 binding 0 `{viewProjection, model}`, pushed per draw. Fragment set 2 binding 0 `albedo` |
| **Stats** | `drawCalls()`, `triangles()` of the current/last frame |

`begin` also works on any colour target, not only the swapchain; the render tests draw into
offscreen textures this way.

**Cameras.** `MeshRenderer` expects projections with Z in [0, 1]. The core's `Camera` component
produces them: `cameraMatrices(…).viewProjection` (see `src/mge/core/README.md` → Cameras).

## Tests
`tests/render/test_graphics.cpp` (label `gpu`):
- **Texture orientation:** a 2×2 image drawn full‑screen has its top‑left pixel at the top‑left.
- **Regions:** `updateRegion` and `copyFromOriginal` land where expected; out‑of‑bounds updates
  are rejected.
- **Missing file:** loading a file that doesn't exist gives an invalid texture.
- **Sandbox scene golden:** `SandboxTerrain` + the PNG atlas + `MeshRenderer`, opaque then
  transparent. Intel and lavapipe differ on about 0.3 % of pixels.
