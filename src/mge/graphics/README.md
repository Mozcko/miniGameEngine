# src/mge/graphics/  →  target `mge_graphics`

**The high‑level renderer on SDL3 GPU** (see [`mge_gpu`](../gpu/README.md)): textures and mesh
drawing with a camera, depth and transparency. It depends on `mge_gpu` and `mge_procgen`. Programs
that link it see `MGE_HAS_GRAPHICS`, and `<mge/mge.h>` then includes it. `mge_add_game` links it
by default (`MODULES platform gpu graphics audio`).

| File | Contents |
|------|----------|
| `Texture2D.h/.cpp` | `Texture2D`: RGBA8 texture from a file (stb_image) or from pixels, region updates, tile copies from the original image |
| `MeshRenderer.h/.cpp` | `MeshRenderer`, `RenderStats`: draws `GpuMesh`es with texture × vertex colour, camera, depth, in three passes |
| `DebugDraw.h/.cpp` | `DebugLines` (CPU line builder: line, aabb, sphere, axes, frustum) and `DebugDraw` (+ GPU buffer, `flush(renderer)`) |
| `Lighting.h` | `Lighting`: sun, hemisphere ambient, linear fog; `Lighting::unlit()` |
| `RenderQueue.h/.cpp` | `MeshPass`, `DrawItem`, `RenderQueue`: per‑frame draw list, sorted by pass and depth |
| `shaders/debug_lines.vert/.frag` | Line list: `viewProjection * position`, vertex colour |
| `shaders/mesh.vert/.frag`, `mesh_cutout.frag` | Embedded GLSL (`mge_add_shaders`): `viewProjection * model * position`; `texture(albedo, uv) * color`, lit and fogged; the cutout variant discards alpha < 0.5 |


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

stb_image is compiled here with `STB_IMAGE_STATIC`, so another copy in the same program (e.g. the
render tests) doesn't clash.

## `MeshRenderer`

```cpp
m_renderer = std::make_unique<mge::MeshRenderer>(device, gpuModule->swapchainFormat());
mge::GpuMesh mesh = device.uploadMesh(mge::primitives::box(glm::vec3(1.0f)));

void onRender() override {
    const mge::GpuFrame* frame = gpuModule->frame();
    if (!frame || !m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height, sky)) return;
    m_renderer->setCamera(glm::perspectiveRH_ZO(fov, aspect, 0.1f, 500.0f) * view);   // Z in [0, 1]
    m_queue.clear();
    m_queue.add({&mesh, model, &atlas, mge::MeshPass::Opaque, center});
    m_queue.add({&leaves, model, &atlas, mge::MeshPass::Cutout, center});
    m_queue.add({&water, model, &atlas, mge::MeshPass::Transparent, center});
    m_queue.sort(cameraPosition, cameraForward);   // by pass, then by depth
    m_renderer->submit(m_queue);                   // or draw(mesh, model, &tex, pass) yourself, in pass order
    m_renderer->end();
}
```

| Feature | Behaviour |
|---------|-----------|
| **Passes** (#31) | **`Opaque`**: depth test + write, no blending. **`Cutout`**: like opaque, but pixels with alpha < 0.5 are discarded (leaves, plants), so holes need no sorting. **`Transparent`**: alpha blending, depth test **without** write (water, glass). Draw in that order |
| **`RenderQueue`** | `add({mesh, model, texture, pass, center})` skips empty meshes. `sort(cameraPosition, cameraForward)` orders by pass, then opaque/cutout **front‑to‑back** (early depth rejection) and transparent **back‑to‑front** (correct blending), by each item's centre. A single mesh's triangles are not sorted, so overlapping translucent surfaces inside one chunk can still blend in the wrong order |
| **Culling** | Back faces culled in every pass, counter‑clockwise = front (MeshData's winding). Double‑sided things, like cross plants, carry both faces as separate quads |
| **Shading** | `base = texture(albedo, uv) * vertexColor`. No texture → 1×1 white, so only the vertex colour shows (colour‑only meshes, AO). Then lighting and fog (below) |
| **Procedural detail** (#34) | `MeshMaterial {detailScale, detailStrength}`, per draw (`draw(…, material)` or `DrawItem::material`). It adds world‑space value noise to the brightness, sampled slightly inside each face so every block has a stable pattern. Meant for **colour‑only** meshes, whose greedy faces would otherwise be large and flat. Default strength 0 = off |
| **Lighting** (#33) | `setLighting(Lighting)`. **Default `Lighting::unlit()`**: base as is. Lit: `base × (ambient(n) + sunColor × max(n·toSun, 0))`, where `ambient` mixes `groundAmbient` → `skyAmbient` by the normal's Y. **Fog**: linear from `fogStart` to `fogEnd` (distance to the camera passed in `setCamera(viewProjection, position)`), towards `fogColor`; use the sky/clear colour |
| **Sampler** | Nearest filtering, clamp to edge (pixel‑art look, as the GL renderer) |
| **Depth** | Owned by the renderer, `device.depthFormat()` (D32/D24), recreated when the target size changes (window resize) |
| **Uniforms** | Vertex set 1 binding 0 `{viewProjection, model, material}`, pushed per draw. Fragment set 2 binding 0 `albedo`; set 3 binding 0 `Lighting` (all `vec4`, so std140 rules don't matter), pushed when it changes or the command buffer is new |
| **Stats** | `stats()`: draw calls and triangles **per pass** (`RenderStats`, indexed by `MeshPass`), plus totals; `drawCalls()` and `triangles()` are shortcuts |

`begin` also works on any colour target, not only the swapchain; the render tests draw into
offscreen textures this way.

**Cameras.** `MeshRenderer` expects projections with Z in [0, 1]. The core's `Camera` component
produces them: `cameraMatrices(…).viewProjection` (see `src/mge/core/README.md` → Cameras).

## Debug draw – `<mge/graphics/DebugDraw.h>` (#38)

```cpp
m_debug = std::make_unique<mge::DebugDraw>(device);
// every frame, between renderer.begin and end (after submit):
m_debug->clear();
m_debug->aabb(chunkBounds, {0.3f, 1, 0.4f, 0.5f});
m_debug->frustum(frozenViewProjection, {1, 0.5f, 0.1f, 1});
m_debug->axes(origin, 2.0f);
m_debug->flush(*m_renderer);
```

- **`DebugLines`** builds the vertices on the CPU (testable without a GPU) as pairs of
  `GpuVertex` (position + colour):
  - `line`;
  - `aabb`: 12 edges, nothing for an empty box;
  - `sphere`: 3 circles;
  - `axes`: X red, Y green, Z blue;
  - `frustum(viewProjection)`: 12 edges from the inverse matrix, for Z in [0, 1].
- **`DebugDraw::flush`** uploads to a buffer that grows as needed, with `uploadToBuffer(…, cycle =
  true)` so the previous frame's data isn't overwritten while in use, then calls
  `MeshRenderer::drawLines`.
- **The line pipeline** is a line list, depth‑tested but not depth‑writing, with no culling and
  alpha blending. Lines are hidden by the scene, and later mesh draws are unaffected.

## Tests
`tests/render/test_graphics.cpp` (label `gpu`):
- **Texture orientation:** a 2×2 image drawn full‑screen has its top‑left pixel at the top‑left.
- **Regions:** `updateRegion` and `copyFromOriginal` land where expected; out‑of‑bounds updates
  are rejected.
- **Missing file:** loading a file that doesn't exist gives an invalid texture.
- **Sandbox scene golden:** `SandboxTerrain` + the PNG atlas + `MeshRenderer` through the queue.
  Intel and lavapipe differ on about 0.3 % of pixels.
- **`RenderQueue` order** (CPU): passes, front‑to‑back and back‑to‑front, empty meshes skipped.
- **Lit colour‑only voxels golden:** greedy + AO hills with sun, ambient and fog. The shape is
  readable without textures; `Lighting::unlit()` keeps the older goldens identical.
- **Procedural sandbox golden:** the same sandbox scene drawn with `sandbox::buildProceduralAtlas()`,
  so no image is loaded. The test also writes `render_output/procedural_atlas.png` for review.
- **Colour detail golden:** greedy colour‑only steps with `MeshMaterial` detail and lighting.
- **Debug lines:** CPU counts per shape and frustum corners; a golden of a box, sphere and axes
  around a cube (lines hidden by depth; Intel vs lavapipe ≈ 0.5 % different).
- **Cutout scene golden:** leaves in front of water, a rose and a flower over grass, added to the
  queue in reverse order on purpose.
