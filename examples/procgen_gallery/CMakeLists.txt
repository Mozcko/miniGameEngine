# Procedural gallery: exports mge_procgen and mge_voxel meshes to OBJ in <build>/gallery/ (headless).
mge_add_game(procgen_gallery
    HEADLESS
    SOURCES ProcgenGallery.cpp
    LIBRARIES mge_procgen mge_voxel
    SMOKE_FRAMES 1)
