#version 450
// Triangle without vertex buffers: positions come from gl_VertexIndex.
// Vertex uniform: set 1, binding 0 (SDL GPU convention, see src/mge/gpu/Shader.h).

layout(set = 1, binding = 0) uniform Params {
    float angle;  // Radians
    float aspect; // width / height, to avoid distortion
};

layout(location = 0) out vec3 outColor;

// SDL GPU NDC: Y points up (SDL converts internally for Vulkan)
const vec2 kPositions[3] = vec2[](vec2(0.0, 0.6), vec2(-0.6, -0.45), vec2(0.6, -0.45));
const vec3 kColors[3] = vec3[](vec3(1.0, 0.25, 0.2), vec3(0.2, 0.9, 0.3), vec3(0.25, 0.4, 1.0));

void main() {
    vec2 p = kPositions[gl_VertexIndex];
    float c = cos(angle);
    float s = sin(angle);
    p = vec2(c * p.x - s * p.y, s * p.x + c * p.y);
    gl_Position = vec4(p.x / aspect, p.y, 0.0, 1.0);
    outColor = kColors[gl_VertexIndex];
}
