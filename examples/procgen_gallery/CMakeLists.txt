# Galería procedural: exporta las mallas de mge_procgen a OBJ en <build>/gallery/ (headless).
mge_add_game(procgen_gallery
    HEADLESS
    SOURCES ProcgenGallery.cpp
    LIBRARIES mge_procgen
    SMOKE_FRAMES 1)
