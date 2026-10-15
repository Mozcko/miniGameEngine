cmake_minimum_required(VERSION 3.21)
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
option(MGE_BUILD_MODULES "Compilar los módulos con SDL/GPU (platform, render, audio, app) y el juego. OFF = solo mge_core" ON)

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
    find_package(SDL2 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(SDL2_mixer REQUIRED)
    find_package(SDL2_ttf REQUIRED)
endif()

# --- Motor (src/mge) y juegos ---
add_subdirectory(src/mge)
if(MGE_BUILD_MODULES)
    add_subdirectory(games/sandbox)
endif()

# --- Tests ---
if(MGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
