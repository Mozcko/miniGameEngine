# Shaders and GLSL Include Library

Shaders in `miniGameEngine` are written in GLSL (Vulkan dialect) and compiled to SPIR-V at build time via `mge_add_shaders` (`cmake/MgeShaders.cmake`). The compiled bytecode is embedded directly into C++ headers (`k<ShaderName><Stage>`), so no shader source or binary files are loaded from disk at runtime.

## Shader `#include` Support

Shaders can share common algorithms and data structures using GLSL `#include` directives.

### Enabling Includes
To use `#include` in a shader, enable the Google include extension near the top of the file:
```glsl
#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/lighting.glsl"
#include "mge/sdf.glsl"
#include "mge/raymarch.glsl"
```

### Include Search Paths
`mge_add_shaders` automatically adds the standard engine shader include path to `glslang`:
- `${PROJECT_SOURCE_DIR}/src/mge/graphics/shaders/include` (resolves `#include "mge/..."`)

Additional search directories can be specified per target using the `INCLUDE_DIRS` argument of `mge_add_shaders`:
```cmake
mge_add_shaders(my_target
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/custom_includes"
    SHADERS custom_shader.frag)
```

### Dependency Tracking
`mge_add_shaders` passes `--depfile` to `glslang` and wires the resulting `.d` depfile into CMake's `add_custom_command(DEPFILE ...)`. Modifying any included header automatically triggers recompilation of all shaders that depend on it, without requiring a clean build or manual CMake reconfiguration.

---

## Available Shader Libraries (`include/mge/`)

### 1. `mge/lighting.glsl`
Common directional lighting, hemispherical ambient lighting, and distance fog. Used by `mesh.frag` and `mesh_cutout.frag`.

- **Uniform block** (`set = 3, binding = 0`):
  ```glsl
  layout(set = 3, binding = 0) uniform Lighting {
      vec4 toSun;          // xyz: normalized direction towards the sun
      vec4 sunColor;       // rgb: sun color and intensity
      vec4 skyAmbient;     // rgb: ambient color for upward normals
      vec4 groundAmbient;  // rgb: ambient color for downward normals
      vec4 fogColor;       // rgb: fog color
      vec4 fogParams;      // x: start, y: end, z: 1.0 = fog enabled
      vec4 cameraPosition; // xyz: camera world position
  };
  ```
- **Functions**:
  - `vec3 calculateLighting(vec3 albedo, vec3 normal)`: Computes hemisphere ambient + Lambertian sun diffuse.
  - `float calculateFogFactor(vec3 worldPosition)`: Computes linear fog factor in `[0, 1]` based on distance to `cameraPosition`.
  - `vec3 applyFog(vec3 litColor, vec3 worldPosition)`: Blends `litColor` with `fogColor` according to the fog factor.
  - `vec3 applyLightingAndFog(vec3 albedo, vec3 normal, vec3 worldPosition)`: Complete shading pipeline combining lighting and fog.

---

### 2. `mge/sdf.glsl`
GLSL Signed Distance Functions library mirroring the names and mathematical formulations in [`src/mge/procgen/Sdf.h`](../../procgen/Sdf.h) to ensure exact CPU/GPU correspondence.

- **Primitives** (centered at origin):
  - `float sdSphere(vec3 p, float radius)`
  - `float sdBox(vec3 p, vec3 halfExtents)`
  - `float sdRoundBox(vec3 p, vec3 halfExtents, float cornerRadius)`
  - `float sdTorus(vec3 p, float majorRadius, float minorRadius)`
  - `float sdCapsule(vec3 p, vec3 a, vec3 b, float radius)`
  - `float sdCylinder(vec3 p, float radius, float halfHeight)`
  - `float sdPlane(vec3 p, vec3 normal, float offset)`
- **Boolean Operations & Smoothing**:
  - `float opUnion(float d1, float d2)`
  - `float opSubtract(float d1, float d2)`
  - `float opIntersect(float d1, float d2)`
  - `float opSmoothUnion(float d1, float d2, float k)`
  - `float opSmoothSubtract(float d1, float d2, float k)`
  - `float opSmoothIntersect(float d1, float d2, float k)`
  - `float opRound(float d, float radius)`
- **Domain Transformations**:
  - `vec3 opRepeat(vec3 p, vec3 period)`: Periodic repetition along axes with `period > 0`.
  - `vec3 opTwist(vec3 p, float radiansPerUnit)`: Torsional twist around the Y axis.

---

### 3. `mge/raymarch.glsl`
Utilities for distance field raymarching, normal estimation, ambient occlusion, and soft shadows.

- **Requirements**:
  The shader must provide a scene distance evaluation function matching the prototype:
  ```glsl
  float map(vec3 p);
  ```
- **Functions**:
  - `float raymarch(vec3 ro, vec3 rd, float tMax, out int steps)`: Marches a ray from `ro` along unit direction `rd` up to distance `tMax`. Returns the distance `t` to the hit surface, or `-1.0` if no surface was hit.
  - `float raymarch(vec3 ro, vec3 rd, float tMax)`: Overload without step count output.
  - `float raymarch(vec3 ro, vec3 rd, float tMin, float tMax, out int steps)`: Overload with custom initial ray distance `tMin`.
  - `vec3 calcNormal(vec3 p)`: Estimates the surface gradient normal using the 4-sample tetrahedron technique (4 `map()` evaluations instead of 6).
  - `float calcAO(vec3 p, vec3 n)`: Estimates ambient occlusion by sampling along the normal `n`.
  - `float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k)`: Soft shadow penumbra calculation (`k` typically 8.0–32.0; higher `k` = sharper shadows).
  - `float softShadow(vec3 ro, vec3 rd, float mint, float maxt)`: Overload with default `k = 16.0`.
- **Configurable Preprocessor Macros**:
  - `MGE_RAYMARCH_MAX_STEPS` (default: 128): Maximum iteration steps during raymarching.
  - `MGE_RAYMARCH_SURF_DIST` (default: 0.001): Surface hit threshold.
  - `MGE_RAYMARCH_SHADOW_STEPS` (default: 64): Maximum iterations for soft shadow rays.

---

### 4. `mge/sdf_pass.glsl`
Main shader template for the `SdfRenderer` render pass. It defines `void main()`, reconstructs the camera ray from `inverseViewProjection`, executes raymarching, discards on miss, evaluates lighting and ambient occlusion, and writes `gl_FragDepth` for correct raster intersection.

- **Uniform block** (`set = 3, binding = 1`):
  ```glsl
  layout(set = 3, binding = 1) uniform SdfUniforms {
      mat4 viewProjection;
      mat4 inverseViewProjection;
      vec4 cameraPositionAndTime; // xyz: cameraPosition, w: time
      vec4 renderSettings;        // x: maxDistance, y: maxSteps
      vec4 params[4];             // Custom game parameters
  };
  ```
- **Convenience Macros**:
  - `sdfTime`: Evaluates to `cameraPositionAndTime.w`.
  - `sdfCameraPosition`: Evaluates to `cameraPositionAndTime.xyz`.
- **Required Game Callbacks**:
  Game shaders must define the scene geometry and color:
  ```glsl
  float map(vec3 p);
  vec3 material(vec3 p);
  ```
