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

# --- Dependencias ---
set(OpenGL_GL_PREFERENCE GLVND)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)

# --- Configuración de Sources ---
# Recolectamos todo el código fuente del MOTOR
file(GLOB_RECURSE ENGINE_SOURCES "src/Engine/*.cpp" "src/Engine/*.h")

# Recolectamos el código fuente del JUEGO (Sandbox)
file(GLOB_RECURSE GAME_SOURCES "src/Game/*.cpp" "src/Game/*.h")

# --- Creación del Ejecutable ---
# Aquí combinamos el motor y el juego en un solo ejecutable por ahora.
# (La separación en librerías se hace en el issue #8)
add_executable(miniGameEngine ${ENGINE_SOURCES} ${GAME_SOURCES})

# --- Include Paths ---
# Permitimos incluir headers como #include "Engine/Graphics/Shader.h"
target_include_directories(miniGameEngine PRIVATE src)
# Headers de terceros como SYSTEM para que sus warnings no nos afecten
target_include_directories(miniGameEngine SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})

# --- Warnings ---
if(MGE_WARNINGS)
    if(MSVC)
        target_compile_options(miniGameEngine PRIVATE /W4 $<$<BOOL:${MGE_WERROR}>:/WX>)
    else()
        target_compile_options(miniGameEngine PRIVATE -Wall -Wextra -Wshadow $<$<BOOL:${MGE_WERROR}>:-Werror>)
    endif()
endif()

# --- Enlazado ---
target_link_libraries(miniGameEngine PRIVATE
    ${SDL2_LIBRARIES}
    SDL2_mixer
    SDL2_ttf
    OpenGL::GL
    GLEW::GLEW
)
