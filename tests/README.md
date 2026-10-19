# tests/

Unit tests for the engine and the sandbox world, using **doctest** with **CTest**.

- **No display needed.** Most tests only run CPU code. The `gpu`‑labelled ones (`mge_gpu_tests`,
  `render_tests`, the windowed smoke tests) render **offscreen**. They are skipped without a GPU
  unless `MGE_REQUIRE_GPU=1` (CI uses Mesa lavapipe, software Vulkan).
- **doctest** is downloaded by CMake (`v2.4.11`, SHA‑256 pinned in `tests/CMakeLists.txt`). It is
  never linked into the game.
- Each `TEST_CASE` is registered as its own CTest test (`doctest_discover_tests`).

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev                      # all tests
./build/tests/game_tests -tc="*determin*" # filter by name (doctest CLI)
```

| File | Covers |
|------|--------|
| `test_main.cpp` | doctest `main()` – no tests here |
| `core/test_time.cpp` | `Time`: scaling, pause, hitch clamp, real clock never negative |
| `core/test_camera.cpp` | `Camera` component: ZO perspective (near→0, far→1, +Y up), view from the transform, pixel ortho corners, world ortho, `CameraSystem` aspect (autoAspect, minimised), the app's viewport + loop, ZO vs GL frustum near plane, `FlyController` look/move/pause/apply and wall sliding |
| `core/test_ecs.cpp` | Entity lifecycle/recycling, components, swap‑and‑pop integrity, views, transform hierarchy (rotation, cycles, destroyed parent), World phases, ECS systems in a headless app |
| `procgen/test_sdf.cpp` | SDF distances, booleans (hard and smooth), transforms, surface nets: radius accuracy, watertight (incl. torus), winding, resolution, empty cases |
| `procgen/test_noise.cpp` | Noise determinism, ranges, Perlin lattice zeros, continuity, Worley, fractals, warp, batch API |
| `procgen/test_mesh_ops.cpp` | Transform (incl. mirror), merge, weld, flip, normals, triangulation, extrude, lathe, sweep |
| `procgen/test_primitives.cpp` | Every primitive: counts, validity, unit normals, watertight, winding vs normals, outward normals, bounds (helpers in `procgen/MeshChecks.h`, reused by later procgen tests) |
| `procgen/test_meshdata.cpp` | `mge_procgen_tests`: MeshData building, validation errors, bounds, OBJ output |
| `core/test_umbrella.cpp` | `<mge/mge.h>` with only `mge_core`: compiles without SDL and doesn't see module headers (`#error` otherwise) |
| `platform/test_umbrella_platform.cpp` | `<mge/mge.h>` with `mge_platform`: `PlatformModule` is available |
| `core/test_input.cpp` | `InputState` edges, mouse delta/wheel, `clear`, `ActionMap` multi‑binding actions and axes |
| `platform/test_sdl_input.cpp` | `mge_platform_tests`: scancode/button translation and `applySdlEvent` with hand‑built SDL events (no SDL init, no window) |
| `voxel/test_block_registry.cpp` | `mge_voxel_tests`: air is id 0, ids/lookup by name, per‑face tiles, atlas UVs |
| `voxel/test_chunk_coords.cpp` | floorDiv/floorMod, world → chunk → local incl. negatives and vertical, `blockAt`, `ChunkData`, hash |
| `voxel/test_voxel_world.cpp` | Load/unload, get/set across chunks (negative, vertical), dirty flags + border propagation, generator determinism |
| `voxel/test_chunk_mesher.cpp` | `tintTextured` (textured → white vertex colour unless tinted); render layers → opaque/cutout/transparent meshes; face culling (watertight single cube, hidden shared faces, 3³ block), transparent split, water/glass rules, cross shapes, neighbours read from the world, atlas UVs per face; seams vs an independent whole‑world reference, re‑mesh after a neighbour loads, greedy (fewer triangles, same area per direction, textured not merged), AO (helpers: `voxel/VoxelTestWorld.h`, `procgen/MeshChecks.h`) |
| `gpu/test_gpu_device.cpp` | `mge_gpu_tests` (label `gpu`): handles are move‑only (compile time), `GpuVertex` interleaving/layout, clear + readback, mesh upload, cancelled command buffers. Offscreen; skipped without a GPU unless `MGE_REQUIRE_GPU=1` (CI, lavapipe). Helper: `gpu/GpuTestContext.h` |
| `render/test_render_scenes.cpp` | `render_tests` (label `gpu`): golden‑image scenes (clear, vertex‑colour triangle, lit boxes with depth), see "Render tests" below. Helpers: `render/RenderTest.h` |
| `gpu/ValidationProbe.cpp` | `gpu_validation_probe` (CTest "gpu: la validación corta…"): triggers a real Vulkan validation error (UBO used but not declared) and passes only if its VUID appears in the output (`PASS_REGULAR_EXPRESSION`), proving that the `FAIL_REGULAR_EXPRESSION "Validation Error"` on every GPU test works. Skipped (77) without the layers unless `MGE_REQUIRE_VALIDATION=1` |
| `render/test_graphics.cpp` | `mge_graphics`: `Texture2D` orientation (image row 0 on top with GL‑style UVs), `updateRegion` / `copyFromOriginal`, missing file; `RenderQueue` order; **sandbox scene golden** (`SandboxTerrain` + PNG atlas + queue); **cutout golden** (leaves in front of water, plants over grass) |
| `gpu/test_gpu_swapchain.cpp` | Vsync default, present‑mode choice (vsync / mailbox / immediate / fallback) and depth‑format choice with fake capabilities; the device's depth format is D24/D32 and works in a render pass |
| `gpu/test_gpu_shaders.cpp` | Embedded SPIR‑V (magic number, stage, 4‑byte alignment); triangle with a fragment uniform drawn and read back (centre = uniform colour, corners = clear). Shaders: `gpu/shaders/test_triangle.*` |
| `game/test_sandbox_blocks.cpp` | Sandbox block registry: solid/opaque rules, render layers (cutout leaves/plants), cross shapes, atlas tiles per face, every block has tiles and colour |
| `game/test_sandbox_terrain.cpp` | `SandboxTerrain` (noise): determinism and seed, stone below / nothing above the world, height range (seas + mountains), every biome > 2 %, block rules (floor never carved, water ≤ 20 and above the surface), caves rate and off switch, decoration rates 1–3 %, far coordinates, trees crossing chunk borders, load‑order independence |

## Render tests (golden images)
`render_tests` (`tests/render/`, label `gpu`) draws scenes with `renderOffscreen()` (128×128 RGBA8,
optional depth). `compareWithGolden(name, image)` compares the result with
`tests/render/golden/<name>.png`, allowing ≤ 1 % of pixels to differ by more than 8/255.

| Scene | Checks |
|-------|--------|
| `clear` | Clear colour |
| `sandbox_scene` | The sandbox through `mge_graphics`: seeded terrain, PNG atlas, the three passes via `RenderQueue` (`render/test_graphics.cpp`) |
| `cutout_leaves_water` | Cutout leaves in front of blended water, a rose and a flower over grass, items added in reverse order (the queue sorts them) |
| `vertex_colors_triangle` | Rasterisation, colour interpolation, back‑face culling of a CCW triangle, NDC Y up (red tip at the top) |
| `lit_boxes_depth` | `MeshData` → `uploadMesh`, `GpuVertex` attributes, uniform matrices (`perspectiveRH_ZO`), depth test between two boxes, directional light |

- **Update:** `MGE_UPDATE_GOLDEN=1 ./build-ci/tests/render_tests` rewrites the references. Look at
  them before committing.
- **On failure:** the image and a red diff go to `<build>/tests/render_output/`.
- **Shaders:** `render/shaders/*`, embedded with `mge_add_shaders`.
- **Adding a scene:** write a `TEST_CASE` that calls `renderOffscreen` and then `checkGolden`, then
  generate its golden.

## Writing tests
- Add `core/test_<topic>.cpp` (engine core) or `game/test_<topic>.cpp`, and list it in `tests/CMakeLists.txt`.
- Link only what you test. `mge_core_tests` (`tests/core/`) links **only `mge_core`**, so it
  also builds with `-DMGE_BUILD_MODULES=OFF`, like `mge_procgen_tests` and `mge_voxel_tests`.
  `game_tests` (`tests/game/`) links `sandbox_world` (headless).
  CTest names are prefixed `core: ` / `sandbox: `.
- **Never** call code that needs a GPU context. Test CPU data instead: `MeshData` from
  `buildChunkMesh`, `ChunkData` from a generator. `Mesh::init` needs OpenGL.
- Allocate chunks on the heap (`std::make_unique<Chunk>`): each one is about 16 KB of blocks.
- Code must build warning‑free under the `ci` preset (`-Werror`) with GCC and Clang. That means
  C++17 only: no C++20 features such as lambda‑captured structured bindings.
- Test names and comments follow the code's convention (Spanish).

## Smoke tests (whole programs)

Besides the unit tests, CTest runs two whole programs:

| CTest name | Program | Needs |
|------------|---------|-------|
| `smoke: headless 1000 frames` | `examples/headless_smoke`: an `Application` with no modules, generating and checking a chunk per frame | nothing (no SDL init, no GPU) |
| `smoke: sandbox offscreen 120 frames` | The real `sandbox`, with `SDL_VIDEODRIVER=offscreen`, `SDL_AUDIODRIVER=dummy`, `MGE_MAX_FRAMES=120` | an OpenGL driver (Mesa llvmpipe is enough). Label **`gpu`**; skip with `ctest -LE gpu` |

CI runs all of them, and runs them again under ASan/UBSan in the `sanitizers` job. For the
sandbox smoke run only, **leak** detection is off: Ubuntu's Mesa EGL driver leaks inside
`eglInitialize`, and LeakSanitizer can't attribute the memory because the driver is already
unloaded. Address and UB checks stay on, and every other test runs with `detect_leaks=1`.

