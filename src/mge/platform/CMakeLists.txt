# Plataforma: ventana SDL + contexto OpenGL (GLEW) e input
add_library(mge_platform STATIC
    Window.h Window.cpp
    InputManager.h InputManager.cpp
)
target_link_libraries(mge_platform PUBLIC mge_core ${SDL2_LIBRARIES} OpenGL::GL GLEW::GLEW)
target_include_directories(mge_platform SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})
mge_set_warnings(mge_platform)
