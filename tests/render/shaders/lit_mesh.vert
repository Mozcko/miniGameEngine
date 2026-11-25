#version 450
// Mesh with GpuVertex (position, normal, color, uv) and a simple directional light
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;

layout(set = 1, binding = 0) uniform Transforms {
    mat4 viewProjection;
    mat4 model;
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;

void main() {
    gl_Position = viewProjection * model * vec4(inPosition, 1.0);
    outNormal = mat3(model) * inNormal; // No non-uniform scaling in these tests
    outColor = inColor;
}
