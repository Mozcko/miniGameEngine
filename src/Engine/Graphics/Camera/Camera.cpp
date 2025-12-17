#include "Camera.h"

namespace Engine {
    Camera::Camera(int width, int height) 
        : m_Width(width), m_Height(height), m_Position(0.0f), 
          m_ProjectionMatrix(1.0f), m_ViewMatrix(1.0f) {}
}