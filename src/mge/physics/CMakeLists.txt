# Módulo de física (headless: core + voxel). Se compila siempre, también sin módulos.
add_library(mge_physics STATIC
    GridQuery.h
    VoxelRaycast.h
    VoxelCollision.h VoxelCollision.cpp
    CharacterController.h CharacterController.cpp
    SpatialHash.h SpatialHash.cpp
    RigidBody.h RigidBody.cpp
    PhysicsWorld.h PhysicsWorld.cpp
)
target_link_libraries(mge_physics PUBLIC mge_core mge_voxel)
mge_set_warnings(mge_physics)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_PHYSICS
target_compile_definitions(mge_physics INTERFACE MGE_HAS_PHYSICS=1)
