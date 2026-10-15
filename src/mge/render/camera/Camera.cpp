#include <mge/render/camera/Camera.h>

namespace mge {
    Camera::Camera(int width, int height)
        : m_position(0.0f), m_projectionMatrix(1.0f), m_viewMatrix(1.0f), m_width(width), m_height(height) {}
}