# Audio (#46): el mezclador de mge_synth sonando en el dispositivo por defecto (SDL3 audio streams)
add_library(mge_audio STATIC
    AudioModule.h AudioModule.cpp
)
target_link_libraries(mge_audio PUBLIC mge_core mge_synth SDL3::SDL3)
mge_set_warnings(mge_audio)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_AUDIO
target_compile_definitions(mge_audio INTERFACE MGE_HAS_AUDIO=1)
