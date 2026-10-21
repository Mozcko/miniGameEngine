# Galería de audio (#50): sonidos del sintetizador → <build>/gallery/audio/*.wav (+ espectrogramas .png).
# Headless: se compila siempre, también sin módulos.
mge_add_game(audio_gallery
    SOURCES AudioGallery.cpp
    LIBRARIES mge_synth mge_procgen
    HEADLESS
    SMOKE_FRAMES 1)
