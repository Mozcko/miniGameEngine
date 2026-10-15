cmake_minimum_required(VERSION 3.24)
project(miniGameEngine LANGUAGES C CXX)

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
option(MGE_WARNINGS "Activar warnings estrictos en nuestro código" ON)
option(MGE_WERROR "Tratar los warnings como errores" OFF)
option(MGE_BUILD_TESTS "Compilar los tests unitarios (doctest + CTest)" ON)
option(MGE_SANITIZERS "Compilar con AddressSanitizer + UndefinedBehaviorSanitizer (Debug)" OFF)
option(MGE_BUILD_BENCHMARKS "Compilar los benchmarks (bench/)" ON)
option(MGE_BUILD_MODULES "Compilar los módulos con SDL/GPU (platform, render, audio, app) y el juego. OFF = solo mge_core" ON)

# --- Sanitizers (todo el build, incluidas dependencias, para que ASan vea todas las asignaciones) ---
if(MGE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# --- Warnings (solo para nuestros targets) ---
function(mge_set_warnings target)
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
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)

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
endif()

# CTest antes de los subdirectorios: juegos y ejemplos también registran tests
if(MGE_BUILD_TESTS)
    enable_testing()
endif()

# --- Motor (src/mge), juegos y ejemplos ---
add_subdirectory(src/mge)
if(MGE_BUILD_MODULES)
    add_subdirectory(game)
    add_subdirectory(examples/headless_smoke)
endif()

# --- Benchmarks ---
if(MGE_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# --- Tests unitarios ---
if(MGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
