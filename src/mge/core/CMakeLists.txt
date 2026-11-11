# Core del motor: SOLO la librería estándar y GLM (header-only). Prohibido incluir SDL o GPU.
add_library(mge_core STATIC
    Application.h Application.cpp
    Assets.h Assets.cpp
    Input.h Input.cpp
    Jobs.h Jobs.cpp
    Log.h Log.cpp
    Module.h
    Profiler.h Profiler.cpp
    Random.h
    SpscQueue.h
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
# JobSystem (std::thread)
find_package(Threads REQUIRED)
target_link_libraries(mge_core PUBLIC Threads::Threads)
target_compile_features(mge_core PUBLIC cxx_std_17)
# Último recurso para encontrar assets/ desde cualquier directorio (ver Assets.h)
if(MGE_SOURCE_ASSET_FALLBACK)
    target_compile_definitions(mge_core PRIVATE MGE_SOURCE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
endif()

if(MGE_PROFILING)
    target_compile_definitions(mge_core PUBLIC MGE_PROFILING=1)
else()
    target_compile_definitions(mge_core PUBLIC MGE_PROFILING=0)
endif()

if(MGE_TRACY)
    target_compile_definitions(mge_core PUBLIC TRACY_ENABLE)
    target_link_libraries(mge_core PUBLIC TracyClient)
endif()

mge_set_warnings(mge_core)
