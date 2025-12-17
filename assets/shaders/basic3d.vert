#version 330 core

// Ahora recibimos vec3 (X, Y, Z)
layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexUV; // Texturas (opcional por ahora)

out vec2 fragmentUV;

// Las 3 matrices sagradas del 3D
uniform mat4 model;      // Dónde está el objeto en el mundo
uniform mat4 view;       // Desde dónde mira la cámara
uniform mat4 projection; // El lente de la cámara (perspectiva)

void main() {
    gl_Position = projection * view * model * vec4(vertexPosition, 1.0);
    fragmentUV = vertexUV;
}