#pragma once

#include <mge/core/math/Geometry.h>

#include <glm/glm.hpp>

namespace mge {

    class Registry;

    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    // Camera component (paired with a Transform: the camera looks down local -Z, with +Y up).
    // Projections use Z in [0, 1] (SDL GPU / Vulkan / D3D / Metal convention).
    struct Camera {
        ProjectionType projection = ProjectionType::Perspective;
        float fovY = 0.7853982f; // Radians (45°), perspective only
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        // Orthographic: visible height in world units (width is derived from aspect)
        float orthoHeight = 10.0f;
        // Pixel-space orthographic (UI): (0, 0) at top-left, y pointing down, 1 unit = 1 pixel.
        // Note: with y pointing down, a CCW quad in pixel coordinates becomes CW on screen.
        bool pixelSpace = false;
        float aspect = 16.0f / 9.0f; // width / height
        bool autoAspect = true;      // CameraSystem matches it to the viewport every frame
    };

    struct CameraMatrices {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::mat4 viewProjection{1.0f};
        glm::vec3 position{0.0f};
        Frustum frustum; // For culling (planes from viewProjection)
    };

    // Camera projection for a viewport (in pixels; also used by pixel-space orthographic)
    glm::mat4 projectionMatrix(const Camera& camera, const glm::uvec2& viewport);
    // Full matrices from the camera's world matrix (Transform::localMatrix or
    // WorldTransform)
    CameraMatrices cameraMatrices(const glm::mat4& cameraWorld, const Camera& camera,
                                  const glm::uvec2& viewport);

    // Matches the aspect ratio of autoAspect cameras to the viewport (Application runs it before the
    // Render phase: window resizes reach all cameras with no game code)
    class CameraSystem {
    public:
        static void update(Registry& registry, const glm::uvec2& viewport);
    };
}
