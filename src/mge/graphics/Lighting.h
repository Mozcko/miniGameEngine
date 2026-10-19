#pragma once

#include <glm/glm.hpp>

namespace mge {

    // Iluminación de la escena para MeshRenderer: un sol direccional (Lambert), luz ambiente hemisférica
    // (cielo arriba, suelo abajo, según la normal) y niebla lineal por distancia a la cámara.
    //   color = albedo × (ambiente(n) + sol × max(n·haciaElSol, 0)), mezclado con la niebla
    struct Lighting {
        glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)); // Hacia dónde viaja la luz
        glm::vec3 sunColor = glm::vec3(0.75f, 0.72f, 0.65f);
        glm::vec3 skyAmbient = glm::vec3(0.50f, 0.55f, 0.65f);    // Normales hacia arriba
        glm::vec3 groundAmbient = glm::vec3(0.30f, 0.27f, 0.24f); // Normales hacia abajo
        bool fog = true;
        glm::vec3 fogColor = glm::vec3(0.5f, 0.7f, 1.0f); // Usar el mismo color que el cielo
        float fogStart = 60.0f;                           // Distancia (unidades del mundo)
        float fogEnd = 140.0f;

        // Sin luz ni niebla: la textura × color de vértice tal cual (el valor por defecto de MeshRenderer)
        static Lighting unlit() {
            Lighting lighting;
            lighting.sunColor = glm::vec3(0.0f);
            lighting.skyAmbient = glm::vec3(1.0f);
            lighting.groundAmbient = glm::vec3(1.0f);
            lighting.fog = false;
            return lighting;
        }
    };
}
