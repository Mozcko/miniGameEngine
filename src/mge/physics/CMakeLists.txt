# Physics module (headless: core + procgen + voxel). Always built, even without modules.
add_library(mge_physics STATIC
    GridQuery.h
    VoxelRaycast.h
    VoxelCollision.h VoxelCollision.cpp
    CharacterController.h CharacterController.cpp
    MeshCollider.h MeshCollider.cpp
    CapsuleCharacterController.h CapsuleCharacterController.cpp
    SpatialHash.h SpatialHash.cpp
    RigidBody.h RigidBody.cpp
    PhysicsWorld.h PhysicsWorld.cpp
)
add_library(mge::physics ALIAS mge_physics)
set_target_properties(mge_physics PROPERTIES EXPORT_NAME physics)
target_include_directories(mge_physics PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_physics PUBLIC mge_core mge_procgen mge_voxel)
mge_set_warnings(mge_physics)

# For <mge/mge.h>: anything linking this module sees MGE_HAS_PHYSICS
target_compile_definitions(mge_physics INTERFACE MGE_HAS_PHYSICS=1)

mge_install_headers(physics
    CapsuleCharacterController.h
    CharacterController.h
    GridQuery.h
    MeshCollider.h
    PhysicsWorld.h
    RigidBody.h
    SpatialHash.h
    VoxelCollision.h
    VoxelRaycast.h
)
