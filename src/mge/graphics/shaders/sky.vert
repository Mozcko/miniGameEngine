#version 450

// Triángulo de pantalla completa generado a partir de gl_VertexIndex (sin buffers de vértices).
// SDL GPU NDC: Y hacia arriba, X en [-1, 1], Y en [-1, 1], Z en [0, 1].
// Dibuja a z = 1 (plano lejano).
const vec2 kPositions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

layout(location = 0) out vec2 outNdc;

void main() {
    vec2 pos = kPositions[gl_VertexIndex];
    outNdc = pos;
    gl_Position = vec4(pos, 1.0, 1.0);
}
