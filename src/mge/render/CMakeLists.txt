# Render OpenGL (legacy: se reemplaza por SDL3 GPU en M3, ver ADR 0001)
add_library(mge_render STATIC
    core/Vertex.h
    core/Texture.h core/Texture.cpp
    core/GLSLProgram.h core/GLSLProgram.cpp
    core/ResourceManager.h core/ResourceManager.cpp
    3d/Mesh.h 3d/Mesh.cpp
    2d/Sprite.h 2d/Sprite.cpp
    2d/Animation.h 2d/Animation.cpp
    2d/GUILabel.h 2d/GUILabel.cpp
    camera/Camera.h camera/Camera.cpp
    camera/Camera2D.h camera/Camera2D.cpp
    camera/Camera3D.h camera/Camera3D.cpp
    camera/FirstPersonCamera.h camera/FirstPersonCamera.cpp
)
# Temporal: las cámaras leen el InputManager de platform (se elimina en #15)
# y Texture/GUILabel usan SDL_Surface/SDL_ttf (se elimina en #9)
target_link_libraries(mge_render PUBLIC mge_core mge_platform SDL2_ttf OpenGL::GL GLEW::GLEW)
mge_set_warnings(mge_render)
