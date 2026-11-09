// Utilidades de trazado de rayos por campos de distancia (SDF raymarching).
// Requiere que el shader defina la función de distancia de la escena:
// float map(vec3 p);
#ifndef MGE_RAYMARCH_GLSL
#define MGE_RAYMARCH_GLSL

#ifndef MGE_RAYMARCH_MAX_STEPS
#define MGE_RAYMARCH_MAX_STEPS 128
#endif

#ifndef MGE_RAYMARCH_SURF_DIST
#define MGE_RAYMARCH_SURF_DIST 0.001
#endif

#ifndef MGE_RAYMARCH_SHADOW_STEPS
#define MGE_RAYMARCH_SHADOW_STEPS 64
#endif

// Declaración previa de la función de escena (debe implementarse en el shader)
float map(vec3 p);

// Marcha un rayo desde ro en dirección unitaria rd dentro del rango [tMin, tMax].
// Retorna la distancia t a la superficie, o -1.0 si no hubo impacto.
float raymarch(vec3 ro, vec3 rd, float tMin, float tMax, out int steps) {
    float t = tMin;
    steps = 0;
    for (int i = 0; i < MGE_RAYMARCH_MAX_STEPS && t < tMax; ++i) {
        steps = i + 1;
        vec3 p = ro + rd * t;
        float d = map(p);
        if (d < MGE_RAYMARCH_SURF_DIST) {
            return t;
        }
        t += d;
    }
    return -1.0;
}

// Marcha un rayo desde t = 0.0 hasta tMax, reportando el número de pasos ejecutados
float raymarch(vec3 ro, vec3 rd, float tMax, out int steps) {
    return raymarch(ro, rd, 0.0, tMax, steps);
}

// Sobrecarga conveniente sin reporte de pasos
float raymarch(vec3 ro, vec3 rd, float tMax) {
    int dummySteps;
    return raymarch(ro, rd, 0.0, tMax, dummySteps);
}

// Normal en superficie calculada con la técnica del tetraedro (4 evaluaciones de map)
vec3 calcNormal(vec3 p) {
    const vec2 k = vec2(1.0, -1.0);
    const float h = 0.0005;
    return normalize(
        k.xyy * map(p + k.xyy * h) +
        k.yyx * map(p + k.yyx * h) +
        k.yxy * map(p + k.yxy * h) +
        k.xxx * map(p + k.xxx * h)
    );
}

// Oclusión ambiental basada en muestreo hemisférico a lo largo de la normal
float calcAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = map(p + n * h);
        occ += (h - d) * sca;
        sca *= 0.95;
    }
    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}

// Sombra suave en raymarching (k = factor de penumbra, típicamente entre 8.0 y 32.0)
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;
    for (int i = 0; i < MGE_RAYMARCH_SHADOW_STEPS && t < maxt; ++i) {
        float h = map(ro + rd * t);
        if (h < 0.001) {
            return 0.0;
        }
        res = min(res, k * h / t);
        t += h;
    }
    return clamp(res, 0.0, 1.0);
}

// Sobrecarga con penumbra por defecto k = 16.0
float softShadow(vec3 ro, vec3 rd, float mint, float maxt) {
    return softShadow(ro, rd, mint, maxt, 16.0);
}

#endif // MGE_RAYMARCH_GLSL
