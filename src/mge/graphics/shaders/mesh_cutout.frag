#version 450
// Como mesh.frag, pero los píxeles casi transparentes se descartan (hojas, plantas): la pasada cutout
// escribe profundidad, así que no hace falta ordenar.
layout(set = 2, binding = 0) uniform sampler2D albedo;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 color = texture(albedo, inUv) * inColor;
    if (color.a < 0.5) discard;
    outColor = vec4(color.rgb, 1.0);
}
