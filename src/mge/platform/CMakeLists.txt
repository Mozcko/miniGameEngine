# Plataforma: ventana SDL e input
add_library(mge_platform STATIC
    Window.h Window.cpp
    SdlInput.h SdlInput.cpp
    PlatformModule.h PlatformModule.cpp
)
target_link_libraries(mge_platform PUBLIC mge_core SDL3::SDL3)
mge_set_warnings(mge_platform)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_PLATFORM
target_compile_definitions(mge_platform INTERFACE MGE_HAS_PLATFORM=1)
