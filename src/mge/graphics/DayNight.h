#pragma once

#include <mge/graphics/Lighting.h>

#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

namespace mge {

    // Sky configuration for a given point in time
    struct SkySettings {
        glm::vec3 zenithColor{0.18f, 0.42f, 0.85f};
        glm::vec3 horizonColor{0.55f, 0.72f, 0.95f};
        glm::vec3 groundColor{0.25f, 0.25f, 0.22f};
        glm::vec3 sunDirection =
            glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)); // Direction light travels toward
        glm::vec3 sunColor{1.0f, 0.98f, 0.92f};
        float sunSize = 0.9995f; /*cos of angular radius*/
        float sunGlow = 8.0f;
        float starsIntensity = 0.0f; // 0 during daytime
        std::uint32_t starsSeed = 1337;
    };

    // Headless day/night state (testable without GPU): hour 0..24 → coherent sky + lighting
    struct DayNight {
        float hour = 10.0f; // 0..24
        float dayLengthSeconds = 600.0f;

        void advance(float dt);  // Advances and wraps to [0, 24)
        SkySettings sky() const; // Interpolated dawn/day/dusk/night colors (keyframe tables)
        Lighting
        lighting(const Lighting& base) const; // Sun, ambient, and fogColor = sky horizonColor (they match!)
    };

} // namespace mge
