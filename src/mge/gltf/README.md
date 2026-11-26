# src/mge/gltf/  →  target `mge_gltf`

**Optional headless glTF 2.0 / GLB model loader** (Part 1 of #200). Depends only on `mge_core`,
`mge_procgen`, and `cgltf` (MIT single-header parser, private implementation detail fetched via
`FetchContent` with pinned `URL_HASH`, no SDL/GPU), so it builds in every configuration,
including `-DMGE_BUILD_MODULES=OFF`, and is tested headless. Games that do not link `mge::gltf`
pay zero binary-size cost (ADR 0004).

Programs that link `mge::gltf` see `MGE_HAS_GLTF`, and `<mge/mge.h>` then includes `GltfModel.h`
and `GltfLoader.h`.

| File | Contents |
|------|----------|
| `GltfModel.h/.cpp` | CPU model representation (`GltfModel`, `GltfScene`, `GltfNode`, `GltfMesh`, `GltfPrimitive`, `GltfMaterial`, `GltfTexture`, `GltfSampler`, `GltfImage`), `computeWorldTransforms()`, `bounds()` |
| `GltfLoader.h/.cpp` | `loadGltfFromMemory()`, `loadGltfFile()` for JSON `.gltf` and binary `.glb` 2.0 via `cgltf` |

## Usage

```cpp
#include <mge/gltf/GltfLoader.h>

auto model = mge::loadGltfFile("models/character.glb"); // relative to assetPath() or absolute
if (model) {
    const std::vector<glm::mat4> world = model->computeWorldTransforms();
    const mge::Aabb bounds = model->bounds();
    // Each primitive holds a validated mge::MeshData plus optional tangents/joints/weights
}
```

## Supported glTF 2.0 features

- Containers: JSON `.gltf` (embedded base64 data URIs or external `.bin` buffers) and binary `.glb` 2.0.
- Mesh attributes: `POSITION`, `NORMAL` (recomputed when missing if `generateMissingNormals` is true),
  `TEXCOORD_0` (V flipped by default to match engine `MeshData` / `Texture2D` bottom-left UVs),
  `COLOR_0` (VEC3/VEC4), `TANGENT` (VEC4), `JOINTS_0` (u8/u16), `WEIGHTS_0` (VEC4).
- Accessors: `SCALAR`/`VEC2`/`VEC3`/`VEC4`, component types `i8`/`u8`/`i16`/`u16`/`u32`/`f32`,
  `normalized` conversion, interleaved `byteStride`, indexed (`u8`/`u16`/`u32`) and non-indexed triangles.
- Materials: `pbrMetallicRoughness` (`baseColorFactor`, `baseColorTexture`, `metallicFactor`,
  `roughnessFactor`, `metallicRoughnessTexture`), `normalTexture` + `normalScale`, `emissiveFactor`,
  `emissiveTexture`, `alphaMode` (`OPAQUE`/`MASK`/`BLEND`), `alphaCutoff`, `doubleSided`.
- Hierarchy: `scenes`, `defaultScene`, `nodes` with TRS (`translation`/`rotation`/`scale`) or 4×4 `matrix`
  decomposition, `computeWorldTransforms()` with cycle protection, `bounds()`.

Tests: `tests/gltf/test_gltf.cpp` → `mge_gltf_tests`.
