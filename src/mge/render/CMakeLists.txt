# Render OpenGL (legacy: se reemplaza por SDL3 GPU en M3, ver ADR 0001)
add_library(mge_render STATIC
    core/Vertex.h
    core/Texture.h core/Texture.cpp
    core/GLSLProgram.h core/GLSLProgram.cpp
    core/ResourceManager.h core/ResourceManager.cpp
    3d/Mesh.h 3d/Mesh.cpp
    2d/Sprite.h 2d/Sprite.cpp
    2d/Animation.h 2d/Animation.cpp
    camera/Camera.h camera/Camera.cpp
    camera/Camera2D.h camera/Camera2D.cpp
    camera/Camera3D.h camera/Camera3D.cpp
    camera/FirstPersonCamera.h camera/FirstPersonCamera.cpp
)
# Las cámaras reciben CameraInput (no leen teclas): render no depende de platform
target_link_libraries(mge_render PUBLIC mge_core mge_procgen OpenGL::GL GLEW::GLEW)
target_include_directories(mge_render SYSTEM PUBLIC ${GLEW_INCLUDE_DIRS})
mge_set_warnings(mge_render)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_RENDER
target_compile_definitions(mge_render INTERFACE MGE_HAS_RENDER=1)
