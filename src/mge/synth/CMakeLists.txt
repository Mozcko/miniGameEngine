# Audio procedural en la CPU (headless: solo core). Se compila siempre, también sin módulos: mezclador,
# síntesis, efectos y música se prueban sin dispositivo de audio. mge_audio lo conecta a SDL3.
add_library(mge_synth STATIC
    Sample.h
    Oscillator.h Oscillator.cpp
    Envelope.h Envelope.cpp
    Filter.h Filter.cpp
    Delay.h Delay.cpp
    Instrument.h
    Mixer.h Mixer.cpp
    Offline.h Offline.cpp
    Sfx.h Sfx.cpp
)
target_link_libraries(mge_synth PUBLIC mge_core)
mge_set_warnings(mge_synth)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_SYNTH
target_compile_definitions(mge_synth INTERFACE MGE_HAS_SYNTH=1)
