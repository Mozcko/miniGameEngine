#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/sdf_pass.glsl"

// Procedural scene with several SDF shapes:
// 1. Smooth-union blobs oscillating over time and intersecting the rasterized cube
// 2. Spatially twisted torus half-buried and intersecting the ground plane
// 3. Periodically repeated column field on the XZ grid
float map(vec3 p) {
    float t = sdfTime;

    // 1. Smooth-union blobs
    vec3 b1Pos = vec3(0.0, 1.6 + sin(t * 1.5) * 0.6, 0.0);
    vec3 b2Pos = vec3(cos(t * 1.2) * 1.4, 1.6 + cos(t * 1.8) * 0.4, sin(t * 1.2) * 1.4);
    float b1 = sdSphere(p - b1Pos, 1.1);
    float b2 = sdSphere(p - b2Pos, 0.8);
    float blobs = opSmoothUnion(b1, b2, 0.5);

    // 2. Torus twisted around the Y axis
    vec3 tp = p - vec3(-6.0, 1.2, 0.0);
    tp = opTwist(tp, 0.5);
    float torus = sdTorus(tp, 2.0, 0.5);

    // 3. Periodically repeated column field in XZ
    vec3 cp = p - vec3(0.0, 2.0, -8.0);
    vec3 rep = opRepeat(cp, vec3(8.0, 0.0, 8.0));
    float columns = sdCylinder(rep, 0.4, 2.5);

    return opUnion(opUnion(blobs, torus), columns);
}

// Materials / albedos for each shape
vec3 material(vec3 p) {
    float distBlobs = length(p - vec3(0.0, 1.6, 0.0));
    float distTorus = length(p - vec3(-6.0, 1.2, 0.0));
    if (distBlobs < 3.2) {
        return vec3(0.92, 0.28, 0.38); // Reddish magenta for the blobs
    } else if (distTorus < 3.2) {
        return vec3(0.20, 0.80, 0.75); // Turquoise for the twisted torus
    } else {
        return vec3(0.78, 0.74, 0.70); // Marble / light stone for the columns
    }
}
