# Bindings de Python para miniGameEngine mediante nanobind

find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)

# Intentar usar el paquete de nanobind instalado en el entorno de Python (ej: pip / scikit-build)
if(NOT nanobind_DIR)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE nanobind_DIR
        RESULT_VARIABLE _nb_res
        ERROR_QUIET
    )
endif()

find_package(nanobind 3.1.0 CONFIG QUIET)
if(NOT nanobind_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nanobind
        URL https://github.com/wjakob/nanobind/archive/refs/tags/v3.1.0.tar.gz
        URL_HASH SHA256=39659890a112e84e7263e4a263c71dd2621f866f7a4386ce95ef410e6c0e5c2f
    )
    FetchContent_GetProperties(nanobind)
    if(NOT nanobind_POPULATED)
        FetchContent_Populate(nanobind)
        if(NOT EXISTS "${nanobind_SOURCE_DIR}/ext/robin_map/include")
            FetchContent_Declare(
                robin_map
                URL https://github.com/Tessil/robin-map/archive/refs/tags/v1.3.0.tar.gz
                URL_HASH SHA256=a8424ad3b0affd4c57ed26f0f3d8a29604f0e1f2ef2089f497f614b1c94c7236
                SOURCE_DIR "${nanobind_SOURCE_DIR}/ext/robin_map"
            )
            FetchContent_Populate(robin_map)
        endif()
        add_subdirectory(${nanobind_SOURCE_DIR} ${nanobind_BINARY_DIR})
    endif()
endif()


nanobind_add_module(_mge NB_STATIC STABLE_ABI NB_SUPPRESS_WARNINGS
    src/App.cpp
    src/Audio.cpp
    src/Ecs.cpp
    src/Math.cpp
    src/Module.cpp
    src/Render.cpp
    src/Voxel.cpp
)

target_include_directories(_mge PRIVATE
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(_mge PRIVATE
    mge_core
    mge_procgen
    mge_voxel
    mge_synth
    mge_physics
)

if(MGE_BUILD_MODULES)
    target_link_libraries(_mge PRIVATE
        mge_platform
        mge_gpu
        mge_graphics
        mge_audio
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(_mge PRIVATE m)
endif()

mge_set_warnings(_mge)
target_compile_options(_mge PRIVATE -Wno-shadow)


# Generar el binario dentro del paquete de desarrollo para poder importar sin instalar
set_target_properties(_mge PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mge"
)

# Instalación para empaquetado con scikit-build-core
install(TARGETS _mge
    LIBRARY DESTINATION mge
    ARCHIVE DESTINATION mge
    RUNTIME DESTINATION mge
)
