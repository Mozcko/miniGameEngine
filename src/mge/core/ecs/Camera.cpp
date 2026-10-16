#include <mge/core/ecs/Camera.h>

#include <mge/core/ecs/Registry.h>

#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    glm::mat4 projectionMatrix(const Camera& camera, const glm::uvec2& viewport) {
        if (camera.projection == ProjectionType::Perspective)
            return glm::perspectiveRH_ZO(camera.fovY, camera.aspect, camera.nearPlane, camera.farPlane);
        if (camera.pixelSpace) {
            // Arriba a la izquierda = (0, 0): bottom = alto, top = 0
            return glm::orthoRH_ZO(0.0f, float(viewport.x), float(viewport.y), 0.0f, camera.nearPlane,
                                   camera.farPlane);
        }
        const float halfHeight = camera.orthoHeight * 0.5f;
        const float halfWidth = halfHeight * camera.aspect;
        return glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.nearPlane,
                               camera.farPlane);
    }

    CameraMatrices cameraMatrices(const glm::mat4& cameraWorld, const Camera& camera,
                                  const glm::uvec2& viewport) {
        CameraMatrices result;
        result.view = glm::inverse(cameraWorld);
        result.projection = projectionMatrix(camera, viewport);
        result.viewProjection = result.projection * result.view;
        result.position = glm::vec3(cameraWorld[3]);
        result.frustum = Frustum::fromViewProjection(result.viewProjection, DepthRange::ZeroToOne);
        return result;
    }

    void CameraSystem::update(Registry& registry, const glm::uvec2& viewport) {
        if (viewport.x == 0 || viewport.y == 0) return; // Ventana minimizada: se conserva el aspecto
        const float aspect = float(viewport.x) / float(viewport.y);
        registry.each<Camera>([aspect](Entity, Camera& camera) {
            if (camera.autoAspect) camera.aspect = aspect;
        });
    }
}
