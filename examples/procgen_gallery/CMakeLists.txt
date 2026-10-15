# Galería procedural: exporta las mallas de mge_procgen y mge_voxel a OBJ en <build>/gallery/ (headless).
mge_add_game(procgen_gallery
    HEADLESS
    SOURCES ProcgenGallery.cpp
    LIBRARIES mge_procgen mge_voxel
    SMOKE_FRAMES 1)
