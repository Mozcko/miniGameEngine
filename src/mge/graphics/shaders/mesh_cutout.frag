#version 450
// Como mesh.frag, pero descarta los píxeles con alfa < 0.5 (pasada cutout: hojas, plantas).
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
layout(location = 4) flat in vec2 inDetail; // x: escala, y: intensidad del detalle procedural
layout(location = 0) out vec4 outColor;

// Ruido de valor 3D (hash de enteros + interpolación suave), en [0, 1]
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
    if (base.a < 0.5) discard; // Cutout: hojas, plantas (escribe profundidad, no hace falta ordenar)
    base.a = 1.0;
    // Detalle procedural (#34): variación de brillo en el espacio del mundo, para mallas de solo color. Se
    // muestrea un poco hacia adentro de la cara para que cada bloque tenga su patrón estable
    if (inDetail.y > 0.0) {
        vec3 p = (inWorldPosition - normalize(inWorldNormal) * 0.01) * inDetail.x;
        float detail = valueNoise(p) * 0.6 + valueNoise(p * 2.7) * 0.4;
        base.rgb *= 1.0 + (detail - 0.5) * 2.0 * inDetail.y;
    }
    vec3 n = normalize(inWorldNormal);
    vec3 ambient = mix(groundAmbient.rgb, skyAmbient.rgb, n.y * 0.5 + 0.5);
    vec3 sun = sunColor.rgb * max(dot(n, toSun.xyz), 0.0);
    vec3 lit = base.rgb * (ambient + sun);

    float distanceToCamera = length(inWorldPosition - cameraPosition.xyz);
    float fog = fogParams.z * clamp((distanceToCamera - fogParams.x) / max(fogParams.y - fogParams.x, 0.001), 0.0, 1.0);
    outColor = vec4(mix(lit, fogColor.rgb, fog), base.a);
}
