# Headless glTF 2.0 / GLB loader (core + procgen only, no SDL or GPU). Always built.
include(FetchContent)
FetchContent_Declare(cgltf
    URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.14.tar.gz
    URL_HASH SHA256=2f3c97a6b989943f50e7d7f228688f6558fe37b1411c13a350e3560d061707d8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cgltf)

add_library(mge_gltf STATIC
    GltfModel.h GltfModel.cpp
    GltfLoader.h GltfLoader.cpp
)
add_library(mge::gltf ALIAS mge_gltf)
set_target_properties(mge_gltf PROPERTIES EXPORT_NAME gltf)
target_include_directories(mge_gltf PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
# cgltf is a private single-header implementation detail (never exposed in public headers)
target_include_directories(mge_gltf SYSTEM PRIVATE "${cgltf_SOURCE_DIR}")
target_link_libraries(mge_gltf PUBLIC mge_core mge_procgen)
mge_set_warnings(mge_gltf)

# For <mge/mge.h>: targets linking this module see MGE_HAS_GLTF
target_compile_definitions(mge_gltf INTERFACE MGE_HAS_GLTF=1)

mge_install_headers(gltf
    GltfModel.h
    GltfLoader.h
)
