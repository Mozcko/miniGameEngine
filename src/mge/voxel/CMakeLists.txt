# Mundos voxel (headless: core + procgen). Se compila siempre, también sin módulos.
add_library(mge_voxel STATIC
    Block.h Block.cpp
    Chunk.h Chunk.cpp
    VoxelWorld.h VoxelWorld.cpp
    ChunkMesher.h ChunkMesher.cpp
    ChunkSnapshot.h ChunkSnapshot.cpp
    ChunkStreamer.h ChunkStreamer.cpp
    ChunkLod.h ChunkLod.cpp
    VoxelRaycast.h VoxelRaycast.cpp
)
add_library(mge::voxel ALIAS mge_voxel)
set_target_properties(mge_voxel PROPERTIES EXPORT_NAME voxel)
target_include_directories(mge_voxel PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_voxel PUBLIC mge_core mge_procgen)
mge_set_warnings(mge_voxel)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_VOXEL
target_compile_definitions(mge_voxel INTERFACE MGE_HAS_VOXEL=1)

mge_install_headers(voxel
    Block.h
    Chunk.h
    ChunkLod.h
    ChunkMesher.h
    ChunkSnapshot.h
    ChunkStreamer.h
    VoxelRaycast.h
    VoxelWorld.h
    detail/MesherCommon.h
)
