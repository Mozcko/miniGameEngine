#include <mge/render/camera/Camera2D.h>

namespace mge {

    Camera2D::Camera2D(int width, int height) : Camera(width, height), m_zoom(1.0f) {
        updateProjection(width, height);
    }

    void Camera2D::updateProjection(int width, int height) {
        m_width = width;
        m_height = height;
        float aspectRatio = (float)width / (float)height;
        // Proyección ortográfica centrada
        m_projectionMatrix = glm::ortho(-aspectRatio * m_zoom, aspectRatio * m_zoom, -m_zoom, m_zoom, -1.0f, 1.0f);
    }

    void Camera2D::update() {
        // En 2D, la vista es simplemente una traslación inversa
        m_viewMatrix = glm::translate(glm::mat4(1.0f), -m_position);
    }
}