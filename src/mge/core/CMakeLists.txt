# Core del motor: SOLO la librería estándar (y GLM más adelante). Prohibido incluir SDL o GPU.
add_library(mge_core STATIC
    Application.h Application.cpp
    Input.h Input.cpp
    Log.h Log.cpp
    Module.h
    Random.h
    Clock.h Clock.cpp
    Time.h
)
target_include_directories(mge_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(mge_core PUBLIC cxx_std_17)
mge_set_warnings(mge_core)
