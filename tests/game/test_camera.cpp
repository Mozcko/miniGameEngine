#include <doctest/doctest.h>
#include <mge/render/camera/Camera3D.h>

using namespace mge;

// La cámara 3D usa el frustum real (KI#3: antes isSphereVisible devolvía siempre true).
// Solo matemática: no necesita contexto OpenGL.
TEST_CASE("Camera3D culling: adelante visible, detrás no") {
    Camera3D camera(1280, 720, glm::vec3(0.0f)); // Yaw -90°: mira hacia -Z
    CHECK(camera.isSphereVisible({0, 0, -20}, 1.0f));
    CHECK_FALSE(camera.isSphereVisible({0, 0, 20}, 1.0f));
    CHECK(camera.isAabbVisible(Aabb({-1, -1, -30}, {1, 1, -28})));
    CHECK_FALSE(camera.isAabbVisible(Aabb({-1, -1, 28}, {1, 1, 30})));
}
