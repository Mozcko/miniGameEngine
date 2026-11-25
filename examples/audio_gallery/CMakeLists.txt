# Audio gallery (#50): synthesizer sounds → <build>/gallery/audio/*.wav (+ .png spectrograms).
# Headless: always built, even without modules.
mge_add_game(audio_gallery
    SOURCES AudioGallery.cpp
    LIBRARIES mge_synth mge_procgen
    HEADLESS
    SMOKE_FRAMES 1)
