# Sandbox: test voxel world for the engine

# World content (blocks and terrain) built on mge_voxel: headless, testable without a window
add_library(sandbox_world STATIC
    SandboxBlocks.h SandboxBlocks.cpp
    SandboxTerrain.h SandboxTerrain.cpp
    SandboxMaterials.h SandboxMaterials.cpp
    SandboxSounds.h SandboxSounds.cpp
)
# Allows #include <game/SandboxTerrain.h>
target_include_directories(sandbox_world PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(sandbox_world PUBLIC mge_voxel mge_physics mge_synth mge_audio)
mge_set_warnings(sandbox_world)

# Game executable (+ run / run-sandbox targets and 120-frame offscreen smoke test)
mge_add_game(sandbox
    SOURCES SandboxApp.cpp
    LIBRARIES sandbox_world
    MODULES platform gpu graphics audio
    SMOKE_FRAMES 120)

# Smoke test with debug helpers (F3/F4) enabled: lines go through Vulkan validation
if(MGE_BUILD_TESTS)
    add_test(NAME "smoke: sandbox offscreen 60 frames (debug draw)" COMMAND sandbox
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    set_tests_properties("smoke: sandbox offscreen 60 frames (debug draw)" PROPERTIES
        TIMEOUT 120 LABELS gpu FAIL_REGULAR_EXPRESSION "Validation Error"
        ENVIRONMENT "MGE_MAX_FRAMES=60;SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;SANDBOX_DEBUG=1;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
