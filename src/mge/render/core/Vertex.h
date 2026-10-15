#pragma once
#include <glm/glm.hpp>

namespace mge {

    struct Vertex {
        glm::vec3 position; // X, Y, Z (En 2D, Z será 0)
        glm::vec4 color;    // R, G, B, A (Para teñir sprites o luces)
        glm::vec2 uv;       // U, V (Coordenadas de textura)

        // Método auxiliar para configurar color por defecto (Blanco)
        void setPosition(float x, float y, float z) { position = glm::vec3(x, y, z); }

        void setUV(float u, float v) { uv = glm::vec2(u, v); }

        void setColor(float r, float g, float b, float a) { color = glm::vec4(r, g, b, a); }
    };
}