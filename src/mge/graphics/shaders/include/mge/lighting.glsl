// Iluminación direccional (sol), ambiente hemisférico y niebla lineal por distancia.
// Usado por mesh.frag y mesh_cutout.frag.
#ifndef MGE_LIGHTING_GLSL
#define MGE_LIGHTING_GLSL

// Fragmento: set 3, binding 0. Todo en vec4 para no depender de las reglas de alineación de std140.
layout(set = 3, binding = 0) uniform Lighting {
    vec4 toSun;          // xyz: dirección hacia el sol (normalizada)
    vec4 sunColor;       // rgb
    vec4 skyAmbient;     // rgb: ambiente para normales hacia arriba
    vec4 groundAmbient;  // rgb: ambiente para normales hacia abajo
    vec4 fogColor;       // rgb
    vec4 fogParams;      // x: inicio, y: fin, z: 1 = niebla activa
    vec4 cameraPosition; // xyz
};

// Calcula la luz ambiente hemisférica + luz solar difusa (Lambert)
vec3 calculateLighting(vec3 albedo, vec3 normal) {
    vec3 n = normalize(normal);
    vec3 ambient = mix(groundAmbient.rgb, skyAmbient.rgb, n.y * 0.5 + 0.5);
    vec3 sun = sunColor.rgb * max(dot(n, toSun.xyz), 0.0);
    return albedo * (ambient + sun);
}

// Factor de niebla en [0, 1] según la distancia al punto de la cámara
float calculateFogFactor(vec3 worldPosition) {
    float distanceToCamera = length(worldPosition - cameraPosition.xyz);
    return fogParams.z * clamp((distanceToCamera - fogParams.x) / max(fogParams.y - fogParams.x, 0.001), 0.0, 1.0);
}

// Aplica niebla mezclando hacia fogColor según la distancia
vec3 applyFog(vec3 litColor, vec3 worldPosition) {
    float fog = calculateFogFactor(worldPosition);
    return mix(litColor, fogColor.rgb, fog);
}

// Aplica iluminación completa y niebla
vec3 applyLightingAndFog(vec3 albedo, vec3 normal, vec3 worldPosition) {
    vec3 lit = calculateLighting(albedo, normal);
    return applyFog(lit, worldPosition);
}

#endif // MGE_LIGHTING_GLSL
