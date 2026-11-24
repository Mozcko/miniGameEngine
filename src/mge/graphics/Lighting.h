#pragma once

#include <glm/glm.hpp>

namespace mge {

    // Scene lighting for MeshRenderer: directional sun (Lambert), hemispheric ambient light
    // (sky above, ground below, based on normal), and linear distance fog from the camera.
    //   color = albedo × (ambient(n) + sun × max(n·toSun, 0)), blended with fog
    struct Lighting {
        glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)); // Direction light travels toward
        glm::vec3 sunColor = glm::vec3(0.75f, 0.72f, 0.65f);
        glm::vec3 skyAmbient = glm::vec3(0.50f, 0.55f, 0.65f);    // Upward normals
        glm::vec3 groundAmbient = glm::vec3(0.30f, 0.27f, 0.24f); // Downward normals
        bool fog = true;
        glm::vec3 fogColor = glm::vec3(0.5f, 0.7f, 1.0f); // Use the same color as the sky
        float fogStart = 60.0f;                           // Distance (world units)
        float fogEnd = 140.0f;

        // No lighting or fog: texture × vertex color as-is (MeshRenderer default)
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
