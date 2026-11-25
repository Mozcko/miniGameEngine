#version 450
// Large triangle covering the center of the image (no vertex buffers)
const vec2 kPositions[3] = vec2[](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));

void main() {
    gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
}
