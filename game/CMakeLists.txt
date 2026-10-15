# Sandbox: mundo voxel de prueba del motor

# Lógica del mundo (bloques y chunks): librería aparte para poder testearla
add_library(sandbox_world STATIC
    Block.h Block.cpp
    Chunk.h Chunk.cpp
)
# Permite #include <game/Chunk.h>
target_include_directories(sandbox_world PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(sandbox_world PUBLIC mge_render)
mge_set_warnings(sandbox_world)

# Ejecutable del juego (+ targets run / run-sandbox y smoke test offscreen de 120 frames)
mge_add_game(sandbox
    SOURCES SandboxApp.cpp
    LIBRARIES sandbox_world
    MODULES platform render audio
    SMOKE_FRAMES 120)
