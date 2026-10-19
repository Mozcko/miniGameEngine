#version 450
// Textura × color del vértice, iluminado (Lighting). Textura: set 2, binding 0 (blanca 1×1 si no hay).
// La iluminación está repetida en mesh.frag y mesh_cutout.frag (el toolchain no tiene #include todavía).
layout(set = 2, binding = 0) uniform sampler2D albedo;

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

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 3) in vec3 inWorldNormal;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 base = texture(albedo, inUv) * inColor;

    vec3 n = normalize(inWorldNormal);
    vec3 ambient = mix(groundAmbient.rgb, skyAmbient.rgb, n.y * 0.5 + 0.5);
    vec3 sun = sunColor.rgb * max(dot(n, toSun.xyz), 0.0);
    vec3 lit = base.rgb * (ambient + sun);

    float distanceToCamera = length(inWorldPosition - cameraPosition.xyz);
    float fog = fogParams.z * clamp((distanceToCamera - fogParams.x) / max(fogParams.y - fogParams.x, 0.001), 0.0, 1.0);
    outColor = vec4(mix(lit, fogColor.rgb, fog), base.a);
}
