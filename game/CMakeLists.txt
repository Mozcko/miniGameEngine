# Sandbox: mundo voxel de prueba del motor

# Contenido del mundo (bloques y terreno) sobre mge_voxel: headless, testeable sin ventana
add_library(sandbox_world STATIC
    SandboxBlocks.h SandboxBlocks.cpp
    SandboxTerrain.h SandboxTerrain.cpp
)
# Permite #include <game/SandboxTerrain.h>
target_include_directories(sandbox_world PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(sandbox_world PUBLIC mge_voxel)
mge_set_warnings(sandbox_world)

# Ejecutable del juego (+ targets run / run-sandbox y smoke test offscreen de 120 frames)
mge_add_game(sandbox
    SOURCES SandboxApp.cpp
    LIBRARIES sandbox_world
    MODULES platform gpu graphics audio
    SMOKE_FRAMES 120)
