# Plataforma: ventana SDL e input
add_library(mge_platform STATIC
    Window.h Window.cpp
    SdlInput.h SdlInput.cpp
    PlatformModule.h PlatformModule.cpp
)
add_library(mge::platform ALIAS mge_platform)
set_target_properties(mge_platform PROPERTIES EXPORT_NAME platform)
target_include_directories(mge_platform PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_platform PUBLIC mge_core SDL3::SDL3)
mge_set_warnings(mge_platform)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_PLATFORM
target_compile_definitions(mge_platform INTERFACE MGE_HAS_PLATFORM=1)

mge_install_headers(platform
    PlatformModule.h
    SdlInput.h
    Window.h
)
