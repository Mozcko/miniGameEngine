# Fileless sound (#46): sine tones from the engine mixer, using only AudioModule
mge_add_game(tone
    SOURCES Tone.cpp
    MODULES audio
    SMOKE_FRAMES 30)
