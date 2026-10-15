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

# --- Dependencias ---
set(OpenGL_GL_PREFERENCE GLVND)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)

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

# --- Motor: librería estática con todo src/Engine (menos el main) ---
file(GLOB_RECURSE ENGINE_SOURCES "src/Engine/*.cpp" "src/Engine/*.h")
list(REMOVE_ITEM ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Main.cpp")

add_library(mge_engine STATIC ${ENGINE_SOURCES})
# Permitimos incluir headers como #include "Engine/Graphics/Shader.h"
target_include_directories(mge_engine PUBLIC src)
# Headers de terceros como SYSTEM para que sus warnings no nos afecten
target_include_directories(mge_engine SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})
target_link_libraries(mge_engine PUBLIC
    ${SDL2_LIBRARIES}
    SDL2_mixer
    SDL2_ttf
    OpenGL::GL
    GLEW::GLEW
)
mge_set_warnings(mge_engine)

# --- Mundo del juego (bloques y chunks): librería aparte para poder testearla ---
add_library(sandbox_world STATIC
    src/Game/Block.cpp src/Game/Block.h
    src/Game/Chunk.cpp src/Game/Chunk.h
)
target_link_libraries(sandbox_world PUBLIC mge_engine)
mge_set_warnings(sandbox_world)

# --- Ejecutable del juego ---
# (La separación completa en módulos del motor se hace en el issue #8)
add_executable(miniGameEngine src/Engine/Main.cpp src/Game/SandboxApp.cpp)
target_link_libraries(miniGameEngine PRIVATE sandbox_world)
mge_set_warnings(miniGameEngine)

# --- Tests ---
if(MGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
