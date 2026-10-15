#include <mge/render/camera/Camera2D.h>

namespace Engine {

    Camera2D::Camera2D(int width, int height) : Camera(width, height), m_Zoom(1.0f) {
        updateProjection(width, height);
    }

    void Camera2D::updateProjection(int width, int height) {
        m_Width = width;
        m_Height = height;
        float aspectRatio = (float)width / (float)height;
        // Proyección ortográfica centrada
        m_ProjectionMatrix = glm::ortho(-aspectRatio * m_Zoom, aspectRatio * m_Zoom, -m_Zoom, m_Zoom, -1.0f, 1.0f);
    }

    void Camera2D::update() {
        // En 2D, la vista es simplemente una traslación inversa
        m_ViewMatrix = glm::translate(glm::mat4(1.0f), -m_Position);
    }
}