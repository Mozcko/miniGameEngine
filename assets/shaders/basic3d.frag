#version 330 core

in vec2 fragmentUV;
out vec4 color;

uniform sampler2D myTexture;
uniform vec4 baseColor; // Color base si no hay textura

void main() {
    // Por ahora mezclamos textura con un color base
    // Si no usas textura, el shader usará pixel blanco * baseColor
    color = texture(myTexture, fragmentUV) * baseColor;
}