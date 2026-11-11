#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/sdf_pass.glsl"

// Hash determinista idéntico a SdfGlideLogic.cpp
uint sectionHashU32(int s, uint salt) {
    uint x = uint(s) ^ (salt * 0x9e3779b9u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float sectionHash(int s, uint salt) {
    return float(sectionHashU32(s, salt) & 0x00ffffffu) / 16777216.0;
}

// Mapa de distancias con signo (SDF) del cañón procedural
float map(vec3 p) {
    // Paredes y piso base
    float d = min(min(p.x + 6.0, 6.0 - p.x), p.y + 3.0);

    const float kL = 24.0;
    int baseS = int(floor((p.z + kL * 0.5) / kL));

    for (int ds = -1; ds <= 1; ++ds) {
        int s = baseS + ds;
        float sz = float(s) * kL;
        float h0 = sectionHash(s, 0u);
        float h1 = sectionHash(s, 1u);
        float h2 = sectionHash(s, 2u);
        float h3 = sectionHash(s, 3u);

        // A. Rocas laterales con unión suave (smooth-union)
        vec3 r1Pos = vec3(-4.8 + h0 * 0.6, -2.0 + h1 * 0.8, sz - 4.0 + (h2 - 0.5) * 3.0);
        float d_rock1 = sdSphere(p - r1Pos, 1.6 + h3 * 0.5);
        d = opSmoothUnion(d, d_rock1, 1.2);

        vec3 r2Pos = vec3(4.8 - h2 * 0.6, -2.0 + h3 * 0.8, sz + 4.0 + (h0 - 0.5) * 3.0);
        float d_rock2 = sdSphere(p - r2Pos, 1.5 + h1 * 0.5);
        d = opSmoothUnion(d, d_rock2, 1.2);

        // B. Pilares con torsión en los costados
        float p1x = -3.8 + (h1 - 0.5) * 0.6;
        float p1z = sz - 7.0;
        vec3 tp1 = opTwist(p - vec3(p1x, 0.0, p1z), (h2 > 0.5 ? 0.35 : -0.35));
        float d_pil1 = sdBox(tp1, vec3(0.55, 6.0, 0.55)) - 0.12;
        d = opSmoothUnion(d, d_pil1, 0.35);

        float p2x = 3.8 - (h3 - 0.5) * 0.6;
        float p2z = sz + 7.0;
        vec3 tp2 = opTwist(p - vec3(p2x, 0.0, p2z), (h0 > 0.5 ? -0.35 : 0.35));
        float d_pil2 = sdBox(tp2, vec3(0.55, 6.0, 0.55)) - 0.12;
        d = opSmoothUnion(d, d_pil2, 0.35);

        // C. Arcos que cruzan el cañón
        float archZ = sz + (h1 - 0.5) * 4.0;
        float archX = (h0 - 0.5) * 1.0;
        float R = 5.2 + h2 * 0.6;
        float r = 0.55 + h3 * 0.15;
        vec3 pArch = p - vec3(archX, -1.0, archZ);
        float d_arch = sdTorus(pArch.xzy, R, r);
        d = opSmoothUnion(d, d_arch, 0.35);
    }
    return d;
}

// Color y material de la superficie del cañón
vec3 material(vec3 p) {
    // Roca estratificada estilo terracota / arenisca
    float strata = sin(p.y * 2.2 + sin(p.z * 0.15) * 1.2) * 0.5 + 0.5;
    vec3 rockBase = mix(vec3(0.68, 0.32, 0.20), vec3(0.42, 0.18, 0.12), strata);

    // Los arcos superiores tienen un toque turquesa / oro ceremonial
    if (p.y > 2.2) {
        float archT = clamp((p.y - 2.2) / 2.5, 0.0, 1.0);
        rockBase = mix(rockBase, vec3(0.18, 0.72, 0.75), archT * 0.75);
    }

    // params[0].xyz contiene la posición del orbe del jugador; da un resplandor cálido a las paredes cercanas
    vec3 playerPos = params[0].xyz;
    float distToPlayer = length(p - playerPos);
    float glow = exp(-distToPlayer * 0.5) * 0.6;
    vec3 glowColor = vec3(1.0, 0.75, 0.3) * glow;

    return rockBase + glowColor;
}
