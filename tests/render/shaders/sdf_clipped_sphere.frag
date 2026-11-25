#version 450
#extension GL_GOOGLE_include_directive : require

#include "mge/sdf_pass.glsl"

// SDF sphere of radius 1.0 centered at (0.0, 1.0, 0.0)
float map(vec3 p) {
    return sdSphere(p - vec3(0.0, 1.0, 0.0), 1.0);
}

// Orange albedo to contrast with the rasterized box
vec3 material(vec3 p) {
    return vec3(0.95, 0.50, 0.15);
}
