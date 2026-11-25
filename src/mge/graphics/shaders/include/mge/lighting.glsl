// Directional lighting (sun), hemispheric ambient, and linear distance fog.
// Used by mesh.frag and mesh_cutout.frag.
#ifndef MGE_LIGHTING_GLSL
#define MGE_LIGHTING_GLSL

// Fragment: set 3, binding 0. All vec4 to avoid relying on std140 alignment rules.
layout(set = 3, binding = 0) uniform Lighting {
    vec4 toSun;          // xyz: direction toward the sun (normalized)
    vec4 sunColor;       // rgb
    vec4 skyAmbient;     // rgb: ambient for upward-facing normals
    vec4 groundAmbient;  // rgb: ambient for downward-facing normals
    vec4 fogColor;       // rgb
    vec4 fogParams;      // x: start, y: end, z: 1 = fog enabled
    vec4 cameraPosition; // xyz
};

// Computes hemispheric ambient + diffuse sunlight (Lambert)
vec3 calculateLighting(vec3 albedo, vec3 normal) {
    vec3 n = normalize(normal);
    vec3 ambient = mix(groundAmbient.rgb, skyAmbient.rgb, n.y * 0.5 + 0.5);
    vec3 sun = sunColor.rgb * max(dot(n, toSun.xyz), 0.0);
    return albedo * (ambient + sun);
}

// Fog factor in [0, 1] based on distance to camera
float calculateFogFactor(vec3 worldPosition) {
    float distanceToCamera = length(worldPosition - cameraPosition.xyz);
    return fogParams.z * clamp((distanceToCamera - fogParams.x) / max(fogParams.y - fogParams.x, 0.001), 0.0, 1.0);
}

// Applies fog by blending toward fogColor based on distance
vec3 applyFog(vec3 litColor, vec3 worldPosition) {
    float fog = calculateFogFactor(worldPosition);
    return mix(litColor, fogColor.rgb, fog);
}

// Applies full lighting and fog
vec3 applyLightingAndFog(vec3 albedo, vec3 normal, vec3 worldPosition) {
    vec3 lit = calculateLighting(albedo, normal);
    return applyFog(lit, worldPosition);
}

#endif // MGE_LIGHTING_GLSL
