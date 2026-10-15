#version 450
// Malla con GpuVertex (posición, normal, color, uv). Uniform del vértice: set 1, binding 0.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;

layout(set = 1, binding = 0) uniform Transforms {
    mat4 viewProjection;
    mat4 model;
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

void main() {
    gl_Position = viewProjection * model * vec4(inPosition, 1.0);
    outColor = inColor;
    outUv = inUv;
}
