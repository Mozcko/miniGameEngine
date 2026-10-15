# Sandbox: mundo voxel de prueba del motor

# Lógica del mundo (bloques y chunks): librería aparte para poder testearla
add_library(sandbox_world STATIC
    Block.h Block.cpp
    Chunk.h Chunk.cpp
)
# Permite #include <sandbox/Chunk.h>
target_include_directories(sandbox_world PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(sandbox_world PUBLIC mge_render)
mge_set_warnings(sandbox_world)

# Ejecutable del juego
add_executable(sandbox SandboxApp.cpp)
target_link_libraries(sandbox PRIVATE sandbox_world mge_platform mge_audio mge_main)
# Se genera en la raíz del build dir (hermano de assets/) para que "../assets/..." funcione
set_target_properties(sandbox PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
mge_set_warnings(sandbox)
