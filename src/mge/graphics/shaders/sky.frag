#version 450

// Procedural sky: vertical gradient from ray.y, sun disc + glow, and
// hash-based stars above the horizon (no textures).
// Note: Procedural clouds omitted in this version, planned for future work.

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform SkyUniforms {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 zenithColor;
    vec4 horizonColor;
    vec4 groundColor;
    vec4 sunDirection; // Direction the light travels toward
    vec4 sunColor;
    vec4 sunParams;    // x: sunSize (cos radius), y: sunGlow, z: starsIntensity, w: starsSeed
};

// Deterministic 3D hash noise for stars
float starHash(vec3 p, float seed) {
    p = fract(p * 0.3183099 + vec3(seed * 0.0013, seed * 0.0027, seed * 0.0041));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main() {
    // Reconstruct world-space view ray (SDL GPU NDC: Y up, Z in [0, 1])
    vec4 target = inverseViewProjection * vec4(inNdc, 1.0, 1.0);
    vec3 worldPos = target.xyz / target.w;
    vec3 ray = normalize(worldPos - cameraPosition.xyz);

    // Sky gradient based on ray.y
    vec3 sky;
    if (ray.y >= 0.0) {
        // Above horizon: smooth transition from horizon to zenith
        float h = clamp(ray.y, 0.0, 1.0);
        float t = pow(1.0 - h, 2.5);
        sky = mix(zenithColor.rgb, horizonColor.rgb, t);
    } else {
        // Below horizon: smooth transition from horizon to ground
        float g = clamp(-ray.y * 2.0, 0.0, 1.0);
        sky = mix(horizonColor.rgb, groundColor.rgb, g);
    }

    // Direction toward the sun (light ray travels along sunDirection, sun is at -sunDirection)
    vec3 sunDir = -normalize(sunDirection.xyz);
    float cosTheta = dot(ray, sunDir);

    float sunSize = sunParams.x; // cos of angular radius (~0.9995)
    float sunGlow = sunParams.y; // glow intensity (~8.0)

    // Sun disc with subtle antialiasing
    float disc = smoothstep(sunSize - 0.0002, sunSize + 0.0002, cosTheta);

    // Sun glow around the disc
    float glow = pow(max(cosTheta, 0.0), 16.0) * (sunGlow * 0.1);

    // If looking below the horizon, the ground occludes the sun
    if (ray.y < 0.0) {
        disc = 0.0;
        glow *= clamp(1.0 + ray.y * 5.0, 0.0, 1.0);
    }
    sky += sunColor.rgb * (disc * 2.0 + glow);

    // Hash-based stars above the horizon (no textures)
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
            // Atmospheric attenuation near the horizon
            star *= clamp(ray.y * 8.0, 0.0, 1.0);
            sky += vec3(star * starsIntensity);
        }
    }

    outColor = vec4(sky, 1.0);
}
