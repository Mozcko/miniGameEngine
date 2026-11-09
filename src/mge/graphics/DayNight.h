#pragma once

#include <mge/graphics/Lighting.h>

#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

namespace mge {

    // Configuración del cielo para un momento dado
    struct SkySettings {
        glm::vec3 zenithColor{0.18f, 0.42f, 0.85f};
        glm::vec3 horizonColor{0.55f, 0.72f, 0.95f};
        glm::vec3 groundColor{0.25f, 0.25f, 0.22f};
        glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)); // Hacia dónde viaja la luz
        glm::vec3 sunColor{1.0f, 0.98f, 0.92f};
        float sunSize = 0.9995f; /*cos del radio*/
        float sunGlow = 8.0f;
        float starsIntensity = 0.0f; // 0 de día
        std::uint32_t starsSeed = 1337;
    };

    // Estado de día/noche headless (testeable sin GPU): hora 0..24 → cielo + iluminación coherentes
    struct DayNight {
        float hour = 10.0f; // 0..24
        float dayLengthSeconds = 600.0f;

        void advance(float dt); // Avanza y envuelve a [0, 24)
        SkySettings
        sky() const; // Colores del amanecer/día/atardecer/noche interpolados (tablas de keyframes)
        Lighting lighting(
            const Lighting& base) const; // Sol, ambiente y fogColor = horizonColor del cielo (¡coinciden!)
    };

} // namespace mge
