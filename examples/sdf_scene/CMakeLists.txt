# SDF raymarching demo scene (#36): SDF shapes (smooth-blended blobs,
# twisted torus, column field) intersecting a rasterized ground plane mesh and cube.
mge_add_game(sdf_scene
    SOURCES SdfScene.cpp
    MODULES platform gpu graphics
    SMOKE_FRAMES 60)
mge_add_shaders(sdf_scene SHADERS scene.frag)
