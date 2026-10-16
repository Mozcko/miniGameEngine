# Core del motor: SOLO la librería estándar y GLM (header-only). Prohibido incluir SDL o GPU.
add_library(mge_core STATIC
    Application.h Application.cpp
    Assets.h Assets.cpp
    Input.h Input.cpp
    Log.h Log.cpp
    Module.h
    Random.h
    Clock.h Clock.cpp
    FlyController.h FlyController.cpp
    Time.h
    math/Geometry.h math/Geometry.cpp
    ecs/Camera.h ecs/Camera.cpp
    ecs/Entity.h
    ecs/Registry.h ecs/Registry.cpp
    ecs/Transform.h ecs/Transform.cpp
    ecs/World.h ecs/World.cpp
)
target_include_directories(mge_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(mge_core PUBLIC cxx_std_17)
# Último recurso para encontrar assets/ desde cualquier directorio (ver Assets.h)
target_compile_definitions(mge_core PRIVATE MGE_SOURCE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
mge_set_warnings(mge_core)
