#version 330 core

layout (location = 0) in vec2 vertexPosition;

// Recibimos la matriz de la cámara desde C++
uniform mat4 P; 

void main() {
    // Multiplicamos la Matriz P por la Posición.
    // Esto transforma la coordenada del mundo a coordenada de pantalla normalizada.
    gl_Position = P * vec4(vertexPosition.x, vertexPosition.y, 0.0, 1.0);
}