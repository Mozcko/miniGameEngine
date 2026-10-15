# Generación procedural en CPU (headless: solo core + GLM). Se compila siempre, también sin módulos.
add_library(mge_procgen STATIC
    MeshData.h MeshData.cpp
    Primitives.h Primitives.cpp
    MeshOps.h MeshOps.cpp
    Noise.h Noise.cpp
    detail/MeshBuilding.h detail/MeshBuilding.cpp
)
target_link_libraries(mge_procgen PUBLIC mge_core)
mge_set_warnings(mge_procgen)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_PROCGEN
target_compile_definitions(mge_procgen INTERFACE MGE_HAS_PROCGEN=1)
