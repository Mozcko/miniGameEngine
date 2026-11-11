cmake_minimum_required(VERSION 3.24)
project(miniGameEngine VERSION 1.0.0 LANGUAGES C CXX)

# --- Estándar de C++ ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Genera compile_commands.json (clangd, clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Por defecto compilamos en Release (con optimizaciones) si nadie eligió otro tipo
get_property(MGE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT MGE_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# --- Opciones ---
option(MGE_INSTALL "Instalar y exportar paquetes CMake para reutilizar el motor" OFF)
option(MGE_WARNINGS "Activar warnings estrictos en nuestro código" ON)
option(MGE_WERROR "Tratar los warnings como errores" OFF)
option(MGE_BUILD_TESTS "Compilar los tests unitarios (doctest + CTest)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_SANITIZERS "Compilar con AddressSanitizer + UndefinedBehaviorSanitizer (Debug)" OFF)
option(MGE_TSAN "Compilar con ThreadSanitizer (Debug; incompatible con MGE_SANITIZERS)" OFF)
option(MGE_BUILD_BENCHMARKS "Compilar los benchmarks (bench/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_EXAMPLES "Compilar los ejemplos (examples/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_SANDBOX "Compilar el sandbox (game/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_MODULES "Compilar los módulos con SDL/GPU (platform, render, audio, app) y el juego. OFF = solo mge_core" ON)
option(MGE_SIZE_OPTIMIZE "Optimizar binarios para tamaño mínimo (-Os, LTO/IPO, gc-sections)" OFF)
option(MGE_BUILD_CLI "Compilar la herramienta CLI mge (tools/mge)" ${PROJECT_IS_TOP_LEVEL})

option(MGE_SOURCE_ASSET_FALLBACK "Usar la carpeta assets/ del código fuente como último recurso en assetPath()" ON)
if(MGE_INSTALL AND MGE_SOURCE_ASSET_FALLBACK)
    message(WARNING "MGE_INSTALL=ON y MGE_SOURCE_ASSET_FALLBACK=ON: installed libraries will contain an absolute path to this source tree; pass -DMGE_SOURCE_ASSET_FALLBACK=OFF for distributable packages")
endif()
option(MGE_PROFILING "Activar instrumentación del profiler (MGE_PROFILE_SCOPE)" ON)
option(MGE_TRACY "Activar integración opcional con Tracy Profiler" OFF)

# Versión del motor (<mge/Version.h>)
configure_file(
    "${miniGameEngine_SOURCE_DIR}/src/mge/Version.h.in"
    "${miniGameEngine_BINARY_DIR}/generated/include/mge/Version.h"
    @ONLY
)

# --- Optimización de tamaño (MGE_SIZE_OPTIMIZE) ---
if(MGE_SIZE_OPTIMIZE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT MGE_IPO_SUPPORTED OUTPUT MGE_IPO_ERROR)
    if(MGE_IPO_SUPPORTED)
        message(STATUS "IPO/LTO soportado y activado")
    else()
        message(WARNING "IPO no soportado: ${MGE_IPO_ERROR}")
    endif()
endif()

# --- ThreadSanitizer: carreras de datos (JobSystem y lo que corre en él) ---
if(MGE_TSAN)
    if(MGE_SANITIZERS)
        message(FATAL_ERROR "MGE_TSAN y MGE_SANITIZERS (ASan) no se pueden combinar")
    endif()
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# --- Sanitizers (todo el build, incluidas dependencias, para que ASan vea todas las asignaciones) ---
if(MGE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# --- Flags de tamaño y warnings (solo para nuestros targets, no para SDL/glslang) ---
function(mge_set_size_flags target)
    if(NOT MGE_SIZE_OPTIMIZE)
        return()
    endif()

    if(MGE_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()

    get_target_property(target_type ${target} TYPE)

    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Os -ffunction-sections -fdata-sections)
        if(target_type STREQUAL "STATIC_LIBRARY")
            target_compile_options(${target} PRIVATE -fvisibility=hidden)
        endif()
    endif()

    if(NOT MSVC AND NOT APPLE)
        if(target_type STREQUAL "EXECUTABLE")
            target_link_options(${target} PRIVATE -Wl,--gc-sections -s)
        endif()
    endif()
endfunction()

function(mge_set_warnings target)
    mge_set_size_flags(${target})
    if(NOT MGE_WARNINGS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${MGE_WERROR}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow $<$<BOOL:${MGE_WERROR}>:-Werror>)
    endif()
endfunction()

# --- Dependencias de los módulos (el core no necesita ninguna) ---
if(MGE_BUILD_MODULES)
    # SDL3: usa el del sistema si existe (ej: SDL3-devel en Fedora); si no, lo compila desde
    # el código fuente (CI en Ubuntu 24.04, que no trae SDL3). Versión fijada + hash.
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    # Extensiones opcionales de X11 que el motor no usa (evita exigir sus paquetes -dev)
    set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz
        URL_HASH SHA256=c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        FIND_PACKAGE_ARGS 3.2 CONFIG
    )
    FetchContent_MakeAvailable(SDL3)
    if(SDL3_FOUND)
        message(STATUS "SDL3: usando la instalación del sistema (${SDL3_VERSION})")
    else()
        message(STATUS "SDL3: compilado desde el código fuente (release-3.4.18)")
    endif()
    if(MGE_INSTALL AND NOT SDL3_FOUND)
        message(FATAL_ERROR "MGE_INSTALL=ON requiere una instalación de SDL3 del sistema (ej: SDL3-devel o libsdl3-dev). SDL3 compilado con FetchContent no es exportable. Instale SDL3 en el sistema o configure con -DMGE_INSTALL=OFF.")
    endif()
endif()

# --- Tracy Profiler (opcional, -DMGE_TRACY=ON) ---
if(MGE_TRACY)
    include(FetchContent)
    FetchContent_Declare(tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.11.1.tar.gz
        URL_HASH SHA256=2c11ca816f2b756be2730f86b0092920419f3dabc7a7173829ffd897d91888a1
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(tracy)
endif()

# mge_add_game(): una llamada por juego (ver cmake/MgeGame.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/MgeGame.cmake)
# mge_add_shaders(): GLSL → SPIR-V embebido (ver cmake/MgeShaders.cmake). Solo con módulos (GPU)
if(MGE_BUILD_MODULES)
    include(${PROJECT_SOURCE_DIR}/cmake/MgeShaders.cmake)
endif()

# CTest antes de los subdirectorios: juegos y ejemplos también registran tests
if(MGE_BUILD_TESTS)
    enable_testing()
endif()

if(MGE_SIZE_OPTIMIZE AND MGE_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# --- Motor (src/mge), juegos y ejemplos ---
add_subdirectory(src/mge)
if(MGE_BUILD_MODULES AND MGE_BUILD_SANDBOX)
    add_subdirectory(game)
endif()
if(MGE_BUILD_EXAMPLES)
    # examples/ decide qué compilar según MGE_BUILD_MODULES (los headless van siempre)
    add_subdirectory(examples)
endif()

# --- Benchmarks ---
if(MGE_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# --- Herramientas (CLI) ---
if(MGE_BUILD_CLI)
    if(WIN32)
        set(MGE_BUILD_CLI OFF CACHE BOOL "Compilar la herramienta CLI mge (tools/mge)" FORCE)
        message(STATUS "mge CLI: desactivado en Windows (tools/mge/Process.cpp requiere POSIX)")
    else()
        add_subdirectory(tools/mge)
    endif()
endif()

# --- Tests unitarios ---
if(MGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Paquete CMake (find_package(mge)) ---
if(MGE_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfigVersion.cmake"
        VERSION ${miniGameEngine_VERSION}
        COMPATIBILITY SameMajorVersion # semver: 1.x es compatible con 1.y (y ≥ x)
    )
    configure_package_config_file(
        "${miniGameEngine_SOURCE_DIR}/cmake/mgeConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfig.cmake"
        INSTALL_DESTINATION lib/cmake/mge
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfigVersion.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeGame.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeShaders.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeEmbedSpirv.cmake"
        DESTINATION lib/cmake/mge
    )
    install(DIRECTORY templates/ DESTINATION share/mge/templates)
endif()
