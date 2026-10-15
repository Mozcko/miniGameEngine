#pragma once
#include <mge/render/camera/Camera.h>

namespace mge {
    class Camera2D : public Camera {
    public:
        Camera2D(int width, int height);
        
        void update() override;
        void updateProjection(int width, int height) override;

        void setZoom(float zoom) { m_zoom = zoom; }
        float getZoom() const { return m_zoom; }

    private:
        float m_zoom;
    };
}