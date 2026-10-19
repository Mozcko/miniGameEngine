# Sandbox: mundo voxel de prueba del motor

# Contenido del mundo (bloques y terreno) sobre mge_voxel: headless, testeable sin ventana
add_library(sandbox_world STATIC
    SandboxBlocks.h SandboxBlocks.cpp
    SandboxTerrain.h SandboxTerrain.cpp
    SandboxMaterials.h SandboxMaterials.cpp
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

# Smoke test con las ayudas de depuración (F3/F4) encendidas: las líneas pasan por la validación de Vulkan
if(MGE_BUILD_TESTS)
    add_test(NAME "smoke: sandbox offscreen 60 frames (debug draw)" COMMAND sandbox
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    set_tests_properties("smoke: sandbox offscreen 60 frames (debug draw)" PROPERTIES
        TIMEOUT 120 LABELS gpu FAIL_REGULAR_EXPRESSION "Validation Error"
        ENVIRONMENT "MGE_MAX_FRAMES=60;SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;SANDBOX_DEBUG=1;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
