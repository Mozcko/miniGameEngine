#pragma once
#include <mge/render/camera/Camera.h>

namespace Engine {
    class Camera2D : public Camera {
    public:
        Camera2D(int width, int height);
        
        void update() override;
        void updateProjection(int width, int height) override;

        void setZoom(float zoom) { m_Zoom = zoom; }
        float getZoom() const { return m_Zoom; }

    private:
        float m_Zoom;
    };
}