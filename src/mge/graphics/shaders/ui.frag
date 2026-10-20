#version 450
// UI 2D (#42): textura × color. UV negativa (Batch2D::kNoTexture) = sin textura (blanco): así los
// rectángulos no cortan el lote de texto
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(set = 2, binding = 0) uniform sampler2D image;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = inUv.x < -0.5 ? vec4(1.0) : texture(image, inUv);
    outColor = texel * inColor;
}
