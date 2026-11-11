#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/sdf_pass.glsl"

// Escena procedural con varias formas SDF:
// 1. Blobs con unión suave (smooth-union) que oscilan con el tiempo y atraviesan el cubo rasterizado
// 2. Toro con torsión espacial (twist) semienterrado e intersectando el plano de suelo
// 3. Campo de columnas repetidas periódicamente (repeat) en la cuadrícula XZ
float map(vec3 p) {
    float t = sdfTime;

    // 1. Blobs con unión suave (smooth union)
    vec3 b1Pos = vec3(0.0, 1.6 + sin(t * 1.5) * 0.6, 0.0);
    vec3 b2Pos = vec3(cos(t * 1.2) * 1.4, 1.6 + cos(t * 1.8) * 0.4, sin(t * 1.2) * 1.4);
    float b1 = sdSphere(p - b1Pos, 1.1);
    float b2 = sdSphere(p - b2Pos, 0.8);
    float blobs = opSmoothUnion(b1, b2, 0.5);

    // 2. Toro con torsión espacial alrededor del eje Y
    vec3 tp = p - vec3(-6.0, 1.2, 0.0);
    tp = opTwist(tp, 0.5);
    float torus = sdTorus(tp, 2.0, 0.5);

    // 3. Campo de columnas repetidas periódicamente en XZ
    vec3 cp = p - vec3(0.0, 2.0, -8.0);
    vec3 rep = opRepeat(cp, vec3(8.0, 0.0, 8.0));
    float columns = sdCylinder(rep, 0.4, 2.5);

    return opUnion(opUnion(blobs, torus), columns);
}

// Materiales / albedos de cada figura
vec3 material(vec3 p) {
    float distBlobs = length(p - vec3(0.0, 1.6, 0.0));
    float distTorus = length(p - vec3(-6.0, 1.2, 0.0));
    if (distBlobs < 3.2) {
        return vec3(0.92, 0.28, 0.38); // Magenta rojizo para los blobs
    } else if (distTorus < 3.2) {
        return vec3(0.20, 0.80, 0.75); // Turquesa para el toro con torsión
    } else {
        return vec3(0.78, 0.74, 0.70); // Mármol / piedra clara para las columnas
    }
}
