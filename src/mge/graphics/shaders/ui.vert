#version 450
// 2D UI (#42): GpuVertex in logical pixels (top-left origin), orthographic projection
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;

layout(set = 1, binding = 0) uniform Screen {
    mat4 projection;
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

void main() {
    gl_Position = projection * vec4(inPosition.xy, 0.0, 1.0);
    outColor = inColor;
    outUv = inUv;
}
