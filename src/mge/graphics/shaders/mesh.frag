#version 450
// Textura × color del vértice. Textura: set 2, binding 0 (una blanca de 1×1 si la malla no tiene).
layout(set = 2, binding = 0) uniform sampler2D albedo;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(albedo, inUv) * inColor;
}
