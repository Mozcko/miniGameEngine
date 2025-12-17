#version 330 core

in vec2 fragmentUV; // <--- Recibimos las coordenadas interpoladas
out vec4 color;

// El "Sampler" es la textura en sí
uniform sampler2D myTexture;

void main() {
    // texture() busca el color del píxel en la coordenada UV
    color = texture(myTexture, fragmentUV);
}