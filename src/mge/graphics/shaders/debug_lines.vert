#version 450
// Debug lines: position and color from GpuVertex (normal and uv unused)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;

layout(set = 1, binding = 0) uniform Camera {
    mat4 viewProjection;
};

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = viewProjection * vec4(inPosition, 1.0);
    outColor = inColor;
}
