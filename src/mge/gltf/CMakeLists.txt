# Headless glTF 2.0 / GLB loader (core + procgen only, no SDL or GPU). Always built.
add_library(mge_gltf STATIC
    GltfModel.h GltfModel.cpp
    GltfLoader.h GltfLoader.cpp
    detail/GltfJson.h detail/GltfJson.cpp
)
add_library(mge::gltf ALIAS mge_gltf)
set_target_properties(mge_gltf PROPERTIES EXPORT_NAME gltf)
target_include_directories(mge_gltf PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_gltf PUBLIC mge_core mge_procgen)
mge_set_warnings(mge_gltf)

# For <mge/mge.h>: targets linking this module see MGE_HAS_GLTF
target_compile_definitions(mge_gltf INTERFACE MGE_HAS_GLTF=1)

mge_install_headers(gltf
    GltfModel.h
    GltfLoader.h
)
