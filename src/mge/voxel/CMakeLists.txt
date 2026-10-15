# Mundos voxel (headless: core + procgen). Se compila siempre, también sin módulos.
add_library(mge_voxel STATIC
    Block.h Block.cpp
    Chunk.h Chunk.cpp
    VoxelWorld.h VoxelWorld.cpp
    ChunkMesher.h ChunkMesher.cpp
)
target_link_libraries(mge_voxel PUBLIC mge_core mge_procgen)
mge_set_warnings(mge_voxel)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_VOXEL
target_compile_definitions(mge_voxel INTERFACE MGE_HAS_VOXEL=1)
