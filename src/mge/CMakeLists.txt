# Módulos del motor. Ver docs/adr/0003-headless-core.md
#   mge_core      -> sin SDL ni GPU (siempre se compila): Application (runner + módulos), Log, Time
#   mge_main      -> el main() del motor (llama a createApplication del juego); solo core
#   mge_platform  -> PlatformModule: ventana, contexto GL, input (SDL3)
#   mge_render    -> OpenGL (legacy hasta #40)
#   mge_audio     -> AudioModule (SDL3)
add_subdirectory(core)
add_subdirectory(app)

if(MGE_BUILD_MODULES)
    add_subdirectory(platform)
    add_subdirectory(render)
    add_subdirectory(audio)
endif()
