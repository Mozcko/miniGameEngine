#version 450

// Fullscreen triangle generated from gl_VertexIndex (no vertex buffers).
// SDL GPU NDC: Y up, X in [-1, 1], Y in [-1, 1], Z in [0, 1].
// Drawn at z = 1 (far plane).
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
