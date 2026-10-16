#pragma once

#include <mge/core/math/Geometry.h>

#include <glm/glm.hpp>

namespace mge {

    class Registry;

    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    // Componente de cámara (va junto a un Transform: la cámara mira hacia -Z local, con +Y arriba).
    // Las proyecciones usan Z en [0, 1] (convención de SDL GPU / Vulkan / D3D / Metal).
    struct Camera {
        ProjectionType projection = ProjectionType::Perspective;
        float fovY = 0.7853982f; // Radianes (45°), solo perspectiva
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        // Ortográfica: alto visible en unidades del mundo (el ancho sale del aspecto)
        float orthoHeight = 10.0f;
        // Ortográfica en píxeles (UI): (0, 0) arriba a la izquierda, y hacia abajo, 1 unidad = 1 píxel.
        // Ojo: con y hacia abajo un quad antihorario en píxeles queda horario en pantalla.
        bool pixelSpace = false;
        float aspect = 16.0f / 9.0f; // ancho / alto
        bool autoAspect = true;      // CameraSystem lo ajusta al viewport cada frame
    };

    struct CameraMatrices {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::mat4 viewProjection{1.0f};
        glm::vec3 position{0.0f};
        Frustum frustum; // Para culling (planos de viewProjection)
    };

    // Proyección de la cámara para un viewport (en píxeles; también lo usa la ortográfica en píxeles)
    glm::mat4 projectionMatrix(const Camera& camera, const glm::uvec2& viewport);
    // Matrices completas a partir de la matriz de mundo de la cámara (Transform::localMatrix o
    // WorldTransform)
    CameraMatrices cameraMatrices(const glm::mat4& cameraWorld, const Camera& camera,
                                  const glm::uvec2& viewport);

    // Ajusta el aspecto de las cámaras con autoAspect al viewport (la Application lo corre antes de la
    // fase Render: el cambio de tamaño de la ventana llega a todas las cámaras sin código del juego)
    class CameraSystem {
    public:
        static void update(Registry& registry, const glm::uvec2& viewport);
    };
}
