#version 450

// Cielo procedural: gradiente vertical según ray.y, disco solar + resplandor y estrellas
// basadas en hash sobre el horizonte (sin texturas).
// Nota: Nubes procedurales omitidas en esta versión, previstas para trabajo futuro.

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform SkyUniforms {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 zenithColor;
    vec4 horizonColor;
    vec4 groundColor;
    vec4 sunDirection; // Hacia dónde viaja la luz
    vec4 sunColor;
    vec4 sunParams;    // x: sunSize (cos radio), y: sunGlow, z: starsIntensity, w: starsSeed
};

// Ruido hash 3D determinista para las estrellas
float starHash(vec3 p, float seed) {
    p = fract(p * 0.3183099 + vec3(seed * 0.0013, seed * 0.0027, seed * 0.0041));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main() {
    // Reconstrucción del rayo de vista en el espacio del mundo (SDL GPU NDC: Y up, Z en [0, 1])
    vec4 target = inverseViewProjection * vec4(inNdc, 1.0, 1.0);
    vec3 worldPos = target.xyz / target.w;
    vec3 ray = normalize(worldPos - cameraPosition.xyz);

    // Gradiente del cielo según ray.y
    vec3 sky;
    if (ray.y >= 0.0) {
        // Sobre el horizonte: transición suave de horizonte a cenit
        float h = clamp(ray.y, 0.0, 1.0);
        float t = pow(1.0 - h, 2.5);
        sky = mix(zenithColor.rgb, horizonColor.rgb, t);
    } else {
        // Debajo del horizonte: transición suave de horizonte a suelo
        float g = clamp(-ray.y * 2.0, 0.0, 1.0);
        sky = mix(horizonColor.rgb, groundColor.rgb, g);
    }

    // Dirección hacia el sol (el rayo de luz viaja en sunDirection, el sol está en -sunDirection)
    vec3 sunDir = -normalize(sunDirection.xyz);
    float cosTheta = dot(ray, sunDir);

    float sunSize = sunParams.x; // cos del radio angular (~0.9995)
    float sunGlow = sunParams.y; // intensidad del resplandor (~8.0)

    // Disco del sol con antialiasing sutil
    float disc = smoothstep(sunSize - 0.0002, sunSize + 0.0002, cosTheta);

    // Resplandor del sol (glow) alrededor del disco
    float glow = pow(max(cosTheta, 0.0), 16.0) * (sunGlow * 0.1);

    // Si miramos debajo del horizonte, el suelo tapa el sol
    if (ray.y < 0.0) {
        disc = 0.0;
        glow *= clamp(1.0 + ray.y * 5.0, 0.0, 1.0);
    }
    sky += sunColor.rgb * (disc * 2.0 + glow);

    // Estrellas basadas en hash arriba del horizonte (sin texturas)
    float starsIntensity = sunParams.z;
    float starsSeed = sunParams.w;
    if (starsIntensity > 0.0 && ray.y > 0.0) {
        vec3 starCoord = ray * 180.0;
        vec3 cell = floor(starCoord);
        vec3 f = fract(starCoord);
        float prob = starHash(cell, starsSeed);
        if (prob > 0.94) {
            vec3 offset = vec3(
                0.15 + 0.7 * starHash(cell + vec3(1.0, 0.0, 0.0), starsSeed),
                0.15 + 0.7 * starHash(cell + vec3(0.0, 1.0, 0.0), starsSeed),
                0.15 + 0.7 * starHash(cell + vec3(0.0, 0.0, 1.0), starsSeed)
            );
            float dist = length(f - offset);
            float brightness = (prob - 0.94) / 0.06;
            float star = smoothstep(0.12, 0.0, dist) * brightness;
            // Atenuación atmosférica cerca del horizonte
            star *= clamp(ray.y * 8.0, 0.0, 1.0);
            sky += vec3(star * starsIntensity);
        }
    }

    outColor = vec4(sky, 1.0);
}
