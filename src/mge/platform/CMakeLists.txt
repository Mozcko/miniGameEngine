# Plataforma: ventana SDL + contexto OpenGL (GLEW) e input
add_library(mge_platform STATIC
    Window.h Window.cpp
    SdlInput.h SdlInput.cpp
    PlatformModule.h PlatformModule.cpp
)
target_link_libraries(mge_platform PUBLIC mge_core SDL3::SDL3 OpenGL::GL GLEW::GLEW)
target_include_directories(mge_platform SYSTEM PUBLIC ${GLEW_INCLUDE_DIRS})
mge_set_warnings(mge_platform)
