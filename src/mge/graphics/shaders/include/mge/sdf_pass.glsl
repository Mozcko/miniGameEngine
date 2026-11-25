// Signed distance field (SDF) raymarching render pass.
// Provides main(), reconstructs the camera ray, evaluates map() and material(),
// computes shared lighting (sun, hemispheric ambient, fog) + calcAO, and writes
// gl_FragDepth in clip space [0, 1] for consistent occlusion and intersection with meshes.
#ifndef MGE_SDF_PASS_GLSL
#define MGE_SDF_PASS_GLSL

#include "mge/lighting.glsl"
#include "mge/sdf.glsl"
#include "mge/raymarch.glsl"

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 1) uniform SdfUniforms {
    mat4 viewProjection;
    mat4 inverseViewProjection;
    vec4 cameraPositionAndTime; // xyz: cameraPosition, w: time
    vec4 renderSettings;        // x: maxDistance, y: maxSteps
    vec4 params[4];             // Free game-defined parameters
};

#define sdfTime (cameraPositionAndTime.w)
#define sdfCameraPosition (cameraPositionAndTime.xyz)

// Functions that must be provided by the game's fragment shader:
float map(vec3 p);
vec3 material(vec3 p);

void main() {
    // 1. Reconstruct camera ray from inverseViewProjection (SDL GPU NDC: [-1, 1], Y up)
    vec4 target = inverseViewProjection * vec4(inNdc, 1.0, 1.0);
    vec3 worldFar = target.xyz / target.w;
    vec3 ro = cameraPositionAndTime.xyz;
    vec3 rd = normalize(worldFar - ro);

    // 2. Raymarching parameters
    float maxDist = renderSettings.x > 0.0 ? renderSettings.x : 200.0;
    int maxIt = int(renderSettings.y > 0.0 ? renderSettings.y : 128.0);
    int steps = 0;
    float t = raymarch(ro, rd, 0.0, maxDist, maxIt, steps);

    // 3. Discard fragment if there was no hit within range
    if (t < 0.0 || t >= maxDist) {
        discard;
    }

    // 4. Shading with shared lighting (Lighting) + ambient occlusion (calcAO)
    vec3 hitPos = ro + rd * t;
    vec3 n = calcNormal(hitPos);
    vec3 albedo = material(hitPos);
    float ao = calcAO(hitPos, n);
    vec3 lit = calculateLighting(albedo, n) * ao;
    vec3 finalColor = applyFog(lit, hitPos);
    outColor = vec4(finalColor, 1.0);

    // 5. Clip-space depth (SDL GPU depth range [0, 1])
    vec4 clipPos = viewProjection * vec4(hitPos, 1.0);
    float depth = clipPos.w > 0.00001 ? (clipPos.z / clipPos.w) : 0.0;
    gl_FragDepth = clamp(depth, 0.0, 1.0);
}

#endif // MGE_SDF_PASS_GLSL
