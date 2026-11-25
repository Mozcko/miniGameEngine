#version 450
// Mesh with GpuVertex (position, normal, color, uv). Vertex uniform: set 1, binding 0.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;

layout(set = 1, binding = 0) uniform Transforms {
    mat4 viewProjection;
    mat4 model;
    vec4 material; // x: procedural detail scale (per world unit), y: intensity (0 = no detail)
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outWorldPosition;
layout(location = 3) out vec3 outWorldNormal;
layout(location = 4) flat out vec2 outDetail;

void main() {
    vec4 world = model * vec4(inPosition, 1.0);
    gl_Position = viewProjection * world;
    outColor = inColor;
    outUv = inUv;
    outWorldPosition = world.xyz;
    outWorldNormal = mat3(model) * inNormal; // No non-uniform scaling (translation/rotation/uniform scale)
    outDetail = material.xy;
}
