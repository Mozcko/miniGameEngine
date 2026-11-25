# Issue #200 — glTF 2.0 Model Import Plan & GitHub Notes

> Note: GitHub API / `gh` CLI was not available in this session, so the plan comment, good-first-issue
> draft, and PR 1 description are recorded here for posting.

---

## 1. Plan Comment for Issue #200

Hi! Picking up **#200 (glTF 2.0 model import: meshes, materials, node hierarchy)** (`size:L`), split into 3 PRs:

1. **PR 1 (`Part of #200`) — Headless `mge_gltf` module (CPU model & parser):**
   - New optional headless static library `src/mge/gltf/` (`mge::gltf`), depending only on `mge_core` and `mge_procgen` (no SDL/GPU, builds under `-DMGE_BUILD_MODULES=OFF`).
   - Data structures (`GltfModel`, `GltfScene`, `GltfNode`, `GltfMesh`, `GltfPrimitive`, `GltfMaterial`, `GltfTexture`, `GltfSampler`, `GltfImage`) with `computeWorldTransforms()` and `bounds()`, plus optional `tangents`/`joints`/`weights` ready for #201.
   - Zero-dependency RFC 8259 JSON parser (`detail/GltfJson.h/.cpp`) + `.gltf` (external `.bin` & base64 data URIs) and binary `.glb` 2.0 loader (`loadGltfFromMemory`, `loadGltfFile` via `mge::assetPath`).
   - Headless unit tests (`mge_gltf_tests`).
2. **PR 2 (`Part of #200`) — GPU upload, rendering & ECS instantiation (`mge_graphics`):**
   - `GpuModel` / `uploadModel(GpuDevice&, const GltfModel&)` uploading `GpuMesh` + decoding embedded/external `GltfImage` via `stb_image` into `Texture2D`, mapping `GltfAlphaMode` to `MeshPass`.
   - `instantiateModel(Registry&, const GltfModel&)` helper for ECS `Transform`/`Parent` hierarchies.
   - Offscreen render test with an in-memory GLB.
3. **PR 3 (`Closes #200`) — C API (`mge_c`), Python bindings (`import mge`), docs & sample:**
   - Expose model loading/rendering in `mge_c` and Python bindings with tests and updated docs (`docs/ASSETS.md`, `docs/C_API.md`, `docs/PYTHON.md`).

Small isolated piece carved out as a `good first issue`: **glTF 2.0 sparse accessor overrides (`accessor.sparse`)** (see below).

---

## 2. Draft Good First Issue (labels: `good first issue`, `help wanted`, `enhancement`, `size:S`)

**Title:** glTF 2.0: support sparse accessor overrides (`accessor.sparse`)

**Parent issue:** Part of #200

### Goal
In glTF 2.0, an accessor may contain a `"sparse"` object (`count`, `indices.{bufferView, byteOffset, componentType}`, `values.{bufferView, byteOffset}`) that overrides specific elements of the base accessor buffer (or a zero-initialized base when `bufferView` is omitted). Currently `GltfLoader.cpp` logs a warning and returns the base data. Implement sparse patching in `mge_gltf` so sparse accessors (e.g. morph targets or compact meshes) load accurately.

### Scope & Files
- `src/mge/gltf/GltfLoader.cpp`: parse `"sparse"` metadata into `AccessorInfo`, and apply sparse index/value overrides inside `readAccessorFloats()` and `readAccessorIndices()` (validating bounds and component types: `UNSIGNED_BYTE`, `UNSIGNED_SHORT`, `UNSIGNED_INT` for sparse indices).
- `tests/gltf/test_gltf.cpp`: add unit tests covering:
  - sparse overrides on a base `VEC3` `POSITION` buffer;
  - sparse accessor with no base `bufferView` (zero-initialized base + sparse patches);
  - out-of-bounds sparse index rejection.

### How to build and test (headless, no SDL/GPU required)
```bash
cmake --preset ci -DMGE_BUILD_MODULES=OFF -G Ninja
cmake --build --preset ci --target mge_gltf_tests
ctest --preset ci -R "gltf:" --output-on-failure
scripts/lint.sh --fix
```

---

## 3. PR 1 Description (`Part of #200`)

**Part of #200**

### What & Why
Adds the headless `mge_gltf` (`mge::gltf`) optional module for parsing glTF 2.0 (`.gltf` and `.glb`) files into CPU `GltfModel` structures (`MeshData`, PBR materials, textures/samplers/images, and node hierarchies). Keeping parsing in a dedicated headless module (no SDL/GPU, no third-party dependencies) respects ADR 0003 and ADR 0004: core stays loader-free, games not using glTF pay zero binary-size cost, and all parsing logic is tested headlessly.

- **C API / Python bindings:** Deferred to PR 3 of #200 once GPU upload and rendering (`GpuModel`, PR 2) are in place, so the public loading + rendering API is exposed cohesively.
- **Good first issue to open:** `glTF 2.0: support sparse accessor overrides (accessor.sparse)` (draft in `docs/plans/200-gltf-plan.md`).

### Verification Needed Before Push
Terminal commands were disabled in this session; please run before pushing:
```bash
cmake --preset ci -G Ninja && cmake --build --preset ci && ctest --preset ci --output-on-failure
cmake -B build-core -DMGE_BUILD_MODULES=OFF -DMGE_WERROR=ON -G Ninja && cmake --build build-core && ctest --test-dir build-core --output-on-failure
cmake --preset asan -G Ninja && cmake --build --preset asan --target mge_gltf_tests && ctest --preset asan -R "gltf:" --output-on-failure
scripts/lint.sh --fix
```
