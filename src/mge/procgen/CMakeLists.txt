# Generación procedural en CPU (headless: solo core + GLM). Se compila siempre, también sin módulos.
add_library(mge_procgen STATIC
    MeshData.h MeshData.cpp
    Image.h Image.cpp
    TextureGen.h TextureGen.cpp
    Font.h Font.cpp BitmapFont8x8.cpp
    Primitives.h Primitives.cpp
    MeshOps.h MeshOps.cpp
    Noise.h Noise.cpp
    Sdf.h Sdf.cpp
    detail/MeshBuilding.h detail/MeshBuilding.cpp
)
add_library(mge::procgen ALIAS mge_procgen)
set_target_properties(mge_procgen PROPERTIES EXPORT_NAME procgen)
target_include_directories(mge_procgen PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_procgen PUBLIC mge_core)
mge_set_warnings(mge_procgen)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_PROCGEN
target_compile_definitions(mge_procgen INTERFACE MGE_HAS_PROCGEN=1)

mge_install_headers(procgen
    Font.h
    Image.h
    MeshData.h
    MeshOps.h
    Noise.h
    Primitives.h
    Sdf.h
    TextureGen.h
    detail/MeshBuilding.h
)
