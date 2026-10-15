#pragma once

#include <glm/glm.hpp>

namespace mge {

    // Convierte una proyección estilo OpenGL (Z de -1 a 1, ej. glm::perspective) a la de SDL GPU
    // (Z de 0 a 1): z' = (z + w) / 2. Mientras las cámaras sigan siendo las del renderer GL (hasta #32).
    inline glm::mat4 depthZeroToOne(const glm::mat4& glProjection) {
        glm::mat4 remap(1.0f);
        remap[2][2] = 0.5f;
        remap[3][2] = 0.5f;
        return remap * glProjection;
    }
}
