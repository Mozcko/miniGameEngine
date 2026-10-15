#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mge {
    class Camera {
    public:
        Camera(int width, int height);
        virtual ~Camera() = default;

        virtual void update() = 0;
        virtual void updateProjection(int width, int height) = 0;

        const glm::vec3& getPosition() const { return m_position; }
        void setPosition(const glm::vec3& position) { m_position = position; }

        const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
        const glm::mat4& getViewMatrix() const { return m_viewMatrix; }

    protected:
        glm::vec3 m_position;
        glm::mat4 m_projectionMatrix;
        glm::mat4 m_viewMatrix;
        int m_width, m_height;
    };
}