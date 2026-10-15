# Audio sobre SDL3 (WAV con SDL_AudioStream). La síntesis procedural llega en M5.
add_library(mge_audio STATIC
    AudioManager.h AudioManager.cpp
    AudioModule.h AudioModule.cpp
)
target_link_libraries(mge_audio PUBLIC mge_core SDL3::SDL3)
mge_set_warnings(mge_audio)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_AUDIO
target_compile_definitions(mge_audio INTERFACE MGE_HAS_AUDIO=1)
