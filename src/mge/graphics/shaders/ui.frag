#version 450
// 2D UI (#42): texture × color. Negative UV (Batch2D::kNoTexture) = untextured (white), so
// rectangles don't break the text batch
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(set = 2, binding = 0) uniform sampler2D image;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = inUv.x < -0.5 ? vec4(1.0) : texture(image, inUv);
    outColor = texel * inColor;
}
