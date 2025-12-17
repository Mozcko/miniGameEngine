cmake_minimum_required(VERSION 3.10)
project(miniGameEngine)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencias ---
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

# --- Include Paths ---
# Permitimos incluir headers como #include "Engine/Graphics/Shader.h"
include_directories(src) 
include_directories(${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})

# --- Creación del Ejecutable ---
# Aquí combinamos el motor y el juego en un solo ejecutable por ahora.
# (Más adelante podríamos separar el motor como una 'add_library')
add_executable(miniGameEngine ${ENGINE_SOURCES} ${GAME_SOURCES})

# --- Enlazado ---
target_link_libraries(miniGameEngine 
    ${SDL2_LIBRARIES} 
    SDL2_mixer
    SDL2_ttf   
    ${OPENGL_LIBRARIES} 
    GLEW::GLEW
)