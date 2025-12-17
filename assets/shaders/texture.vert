#version 330 core

layout (location = 0) in vec2 vertexPosition;
layout (location = 1) in vec2 vertexUV; // <--- Nuevo input

out vec2 fragmentUV; // <--- Se lo pasamos al fragment shader

uniform mat4 P;

void main() {
    gl_Position = P * vec4(vertexPosition.x, vertexPosition.y, 0.0, 1.0);
    
    // Pasamos la coordenada UV directamente
    // (OpenGL interpolará esto para cada píxel)
    fragmentUV = vertexUV;
}