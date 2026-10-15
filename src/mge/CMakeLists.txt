# Módulos del motor. Ver docs/adr/0003-headless-core.md
#   mge_core      -> sin SDL ni GPU (siempre se compila)
#   mge_platform  -> ventana, contexto GL, input (SDL)
#   mge_render    -> OpenGL (legacy hasta #40)
#   mge_audio     -> audio (SDL)
#   mge_app       -> Application: une los módulos
#   mge_main      -> el main() del motor (llama a createApplication del juego)
add_subdirectory(core)

if(MGE_BUILD_MODULES)
    add_subdirectory(platform)
    add_subdirectory(render)
    add_subdirectory(audio)
    add_subdirectory(app)
endif()
