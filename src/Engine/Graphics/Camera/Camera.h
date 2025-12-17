#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
    class Camera {
    public:
        Camera(int width, int height);
        virtual ~Camera() = default;

        virtual void update() = 0;
        virtual void updateProjection(int width, int height) = 0;

        const glm::vec3& getPosition() const { return m_Position; }
        void setPosition(const glm::vec3& position) { m_Position = position; }

        const glm::mat4& getProjectionMatrix() const { return m_ProjectionMatrix; }
        const glm::mat4& getViewMatrix() const { return m_ViewMatrix; }

    protected:
        glm::vec3 m_Position;
        glm::mat4 m_ProjectionMatrix;
        glm::mat4 m_ViewMatrix;
        int m_Width, m_Height;
    };
}