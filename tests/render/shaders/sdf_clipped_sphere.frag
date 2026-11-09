#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/sdf_pass.glsl"

// Esfera SDF de radio 1.0 centrada en (0.0, 1.0, 0.0)
float map(vec3 p) {
    return sdSphere(p - vec3(0.0, 1.0, 0.0), 1.0);
}

// Albedo naranja para contrastar con la caja rasterizada
vec3 material(vec3 p) {
    return vec3(0.95, 0.50, 0.15);
}
