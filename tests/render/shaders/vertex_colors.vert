#version 450
// Triángulo con un color por vértice (interpolado): prueba la rasterización y la interpolación
const vec2 kPositions[3] = vec2[](vec2(0.0, 0.8), vec2(-0.8, -0.7), vec2(0.8, -0.7));
const vec3 kColors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    outColor = kColors[gl_VertexIndex];
}
