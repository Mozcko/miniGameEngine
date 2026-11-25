#version 450
#extension GL_GOOGLE_include_directive : require

// Like mesh.frag, but discards pixels with alpha < 0.5 (cutout pass: leaves, plants).
layout(set = 2, binding = 0) uniform sampler2D albedo;

#include "mge/lighting.glsl"

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 3) in vec3 inWorldNormal;
layout(location = 4) flat in vec2 inDetail; // x: scale, y: procedural detail intensity
layout(location = 0) out vec4 outColor;

// 3D value noise (integer hash + smooth interpolation), in [0, 1]
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y),
               f.z);
}

void main() {
    vec4 base = texture(albedo, inUv) * inColor;
    if (base.a < 0.5) discard; // Cutout: leaves, plants (writes depth, no sorting needed)
    base.a = 1.0;
    // Procedural detail (#34): world-space brightness variation for untextured meshes. Sampled
    // slightly inward from the face so each block gets a stable pattern
    if (inDetail.y > 0.0) {
        vec3 p = (inWorldPosition - normalize(inWorldNormal) * 0.01) * inDetail.x;
        float detail = valueNoise(p) * 0.6 + valueNoise(p * 2.7) * 0.4;
        base.rgb *= 1.0 + (detail - 0.5) * 2.0 * inDetail.y;
    }
    outColor = vec4(applyLightingAndFog(base.rgb, inWorldNormal, inWorldPosition), base.a);
}
