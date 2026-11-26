# --- doctest (development only, never linked into the game) ---
include(FetchContent)
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # We only need the header and scripts/cmake/doctest.cmake: SOURCE_SUBDIR points to a
    # folder without CMakeLists so doctest's CMakeLists is not executed (it requests CMake 3.0,
    # which CMake 4 no longer accepts).
    SOURCE_SUBDIR do-not-configure
)
FetchContent_MakeAvailable(doctest)

add_library(doctest_header INTERFACE)
target_include_directories(doctest_header SYSTEM INTERFACE ${doctest_SOURCE_DIR})

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

# --- Core tests: link only mge_core (no SDL or GPU) ---
add_executable(mge_core_tests
    test_main.cpp
    core/test_application.cpp
    core/test_assets.cpp
    core/test_camera.cpp
    core/test_ecs.cpp
    core/test_geometry.cpp
    core/test_input.cpp
    core/test_jobs.cpp
    core/test_log.cpp
    core/test_random.cpp
    core/test_spsc_queue.cpp
    core/test_log_stripped.cpp
    core/test_profiler.cpp
    core/test_third_person_camera.cpp
    core/test_time.cpp
    core/test_umbrella.cpp
    core/test_version.cpp
)
target_link_libraries(mge_core_tests PRIVATE mge_core doctest_header)
mge_set_warnings(mge_core_tests)
doctest_discover_tests(mge_core_tests TEST_PREFIX "core: ")

# --- Headless module tests (no SDL or GPU): always built ---
add_executable(mge_procgen_tests
    test_main.cpp
    procgen/test_font.cpp
    procgen/test_image.cpp
    procgen/test_meshdata.cpp
    procgen/test_primitives.cpp
    procgen/test_mesh_ops.cpp
    procgen/test_noise.cpp
    procgen/test_sdf.cpp
)
target_link_libraries(mge_procgen_tests PRIVATE mge_procgen doctest_header)
mge_set_warnings(mge_procgen_tests)
doctest_discover_tests(mge_procgen_tests TEST_PREFIX "procgen: ")

add_executable(mge_gltf_tests
    test_main.cpp
    gltf/test_gltf.cpp
)
target_link_libraries(mge_gltf_tests PRIVATE mge_gltf doctest_header)
mge_set_warnings(mge_gltf_tests)
doctest_discover_tests(mge_gltf_tests TEST_PREFIX "gltf: ")

add_executable(mge_synth_tests
    test_main.cpp
    synth/test_mixer.cpp
    synth/test_synth.cpp
    synth/test_offline.cpp
    synth/test_sfx.cpp
    synth/test_music.cpp
)
target_link_libraries(mge_synth_tests PRIVATE mge_synth doctest_header)
mge_set_warnings(mge_synth_tests)
doctest_discover_tests(mge_synth_tests TEST_PREFIX "synth: ")

# "The audio thread does not allocate memory": overrides operator new, so it runs in its own
# executable and is not built with sanitizers (ASan and TSan already override new/delete)
if(NOT MGE_SANITIZERS AND NOT MGE_TSAN)
    add_executable(mge_synth_alloc_tests test_main.cpp synth/test_no_alloc.cpp)
    target_link_libraries(mge_synth_alloc_tests PRIVATE mge_synth doctest_header)
    mge_set_warnings(mge_synth_alloc_tests)
    doctest_discover_tests(mge_synth_alloc_tests TEST_PREFIX "synth: ")
endif()

add_executable(mge_voxel_tests
    test_main.cpp
    voxel/test_block_registry.cpp
    voxel/test_chunk_coords.cpp
    voxel/test_chunk_lod.cpp
    voxel/test_chunk_mesher.cpp
    voxel/test_parallel.cpp
    voxel/test_streaming.cpp
    voxel/test_voxel_raycast.cpp
    voxel/test_voxel_world.cpp
)
target_link_libraries(mge_voxel_tests PRIVATE mge_voxel doctest_header)
mge_set_warnings(mge_voxel_tests)
doctest_discover_tests(mge_voxel_tests TEST_PREFIX "voxel: ")

add_executable(mge_physics_tests
    test_main.cpp
    physics/test_collision.cpp
    physics/test_character.cpp
    physics/test_mesh_collider.cpp
    physics/test_collision_world.cpp
    physics/test_capsule_character.cpp
    physics/test_spatial_hash.cpp
    physics/test_rigid_body.cpp
)
target_link_libraries(mge_physics_tests PRIVATE mge_physics doctest_header)
mge_set_warnings(mge_physics_tests)
doctest_discover_tests(mge_physics_tests TEST_PREFIX "physics: ")

# --- Module tests (require SDL; do not open a window) ---
if(MGE_BUILD_MODULES)
    add_executable(mge_platform_tests
        test_main.cpp
        platform/test_sdl_input.cpp
        platform/test_umbrella_platform.cpp
    )
    target_link_libraries(mge_platform_tests PRIVATE mge_platform doctest_header)
    mge_set_warnings(mge_platform_tests)
    doctest_discover_tests(mge_platform_tests TEST_PREFIX "platform: ")

    # Audio with SDL's dummy driver (no sound card)
    add_executable(mge_audio_tests test_main.cpp audio/test_audio_module.cpp)
    target_link_libraries(mge_audio_tests PRIVATE mge_audio doctest_header)
    mge_set_warnings(mge_audio_tests)
    doctest_discover_tests(mge_audio_tests TEST_PREFIX "audio: "
        PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")

    # GPU (SDL3 GPU, Vulkan). Headless: renders to textures. Skipped without a GPU (MGE_REQUIRE_GPU=1 → fails).
    # With sanitizers, LeakSanitizer is disabled: Mesa drivers leak memory that LSan cannot attribute.
    add_executable(mge_gpu_tests
        test_main.cpp
        gpu/test_gpu_compute.cpp
        gpu/test_gpu_device.cpp
        gpu/test_gpu_noise.cpp
        gpu/test_gpu_shaders.cpp
        gpu/test_gpu_swapchain.cpp
    )
    target_link_libraries(mge_gpu_tests PRIVATE mge_gpu doctest_header)
    mge_add_shaders(mge_gpu_tests SHADERS
        gpu/shaders/test_triangle.vert
        gpu/shaders/test_triangle.frag
        gpu/shaders/test_square.comp
        gpu/shaders/test_multiply.comp
        gpu/shaders/test_sdf_raymarch.comp)

    # Vulkan validation errors appear in test output: CTest fails any GPU test that prints them
    # (FAIL_REGULAR_EXPRESSION). The probe triggers one intentionally and passes only if it is seen.
    add_executable(gpu_validation_probe gpu/ValidationProbe.cpp)
    target_link_libraries(gpu_validation_probe PRIVATE mge_gpu)
    mge_set_warnings(gpu_validation_probe)
    mge_add_shaders(gpu_validation_probe SHADERS gpu/shaders/test_triangle.vert gpu/shaders/test_triangle.frag)
    add_test(NAME "gpu: validation fails on incorrect API usage" COMMAND gpu_validation_probe)
    set_tests_properties("gpu: validation fails on incorrect API usage" PROPERTIES
        LABELS gpu SKIP_RETURN_CODE 77 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1"
        PASS_REGULAR_EXPRESSION "VUID-VkGraphicsPipelineCreateInfo-layout-07988")

    # Render: scenes compared against reference images (tests/render/golden). MGE_UPDATE_GOLDEN=1 rewrites them
    add_executable(render_tests
        test_main.cpp
        render/RenderTest.h render/RenderTest.cpp
        render/test_render_scenes.cpp
        render/test_graphics.cpp
        render/test_text.cpp
        render/test_daynight.cpp
    )
    target_link_libraries(render_tests PRIVATE mge_graphics sandbox_world platformer2d_view doctest_header)
    target_compile_definitions(render_tests PRIVATE
        MGE_TEST_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
        MGE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/render/golden"
        MGE_RENDER_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/render_output")
    mge_set_warnings(render_tests)
    mge_add_shaders(render_tests SHADERS
        render/shaders/vertex_colors.vert render/shaders/vertex_colors.frag
        render/shaders/lit_mesh.vert render/shaders/lit_mesh.frag
        render/shaders/sdf_clipped_sphere.frag)
    doctest_discover_tests(render_tests TEST_PREFIX "render: "
        PROPERTIES LABELS gpu ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1"
                   FAIL_REGULAR_EXPRESSION "Validation Error")
    mge_set_warnings(mge_gpu_tests)
    doctest_discover_tests(mge_gpu_tests TEST_PREFIX "gpu: "
        PROPERTIES LABELS gpu ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1"
                   FAIL_REGULAR_EXPRESSION "Validation Error")
endif()

# --- Sandbox tests (require engine modules) ---
if(MGE_BUILD_MODULES)
    add_executable(game_tests
        test_main.cpp
        game/test_sandbox_blocks.cpp
        game/test_sandbox_terrain.cpp
        game/test_sandbox_audio.cpp
    )
    target_link_libraries(game_tests PRIVATE sandbox_world doctest_header)
    mge_set_warnings(game_tests)
    doctest_discover_tests(game_tests TEST_PREFIX "game: ")

    # 2D Platformer (#45): ASCII levels and physics, headless
    add_executable(platformer_tests test_main.cpp examples/test_platformer.cpp)
    target_link_libraries(platformer_tests PRIVATE platformer2d_logic doctest_header)
    mge_set_warnings(platformer_tests)
    doctest_discover_tests(platformer_tests TEST_PREFIX "platformer: ")

    # SDF Glide demo (#66): SDF canyon, collisions, motes, logic and synth audio, headless
    add_executable(sdf_glide_tests test_main.cpp examples/test_sdf_glide.cpp)
    target_link_libraries(sdf_glide_tests PRIVATE sdf_glide_logic mge_procgen doctest_header)
    mge_set_warnings(sdf_glide_tests)
    doctest_discover_tests(sdf_glide_tests TEST_PREFIX "sdf_glide: ")
endif()

# --- mge CLI tests ---
if(MGE_BUILD_CLI)
    add_executable(mge_cli_tests
        test_main.cpp
        cli/test_cli.cpp
        cli/test_wizard.cpp
    )
    target_link_libraries(mge_cli_tests PRIVATE mge_cli_core doctest_header)
    target_compile_definitions(mge_cli_tests PRIVATE
        MGE_CLI_GOLDEN_DIR="${miniGameEngine_SOURCE_DIR}/tools/mge/tests/golden"
    )
    mge_set_warnings(mge_cli_tests)
    doctest_discover_tests(mge_cli_tests TEST_PREFIX "cli: ")
endif()

# --- C API (mge_c) tests in pure C99 (no doctest) ---
if(MGE_BUILD_MODULES)
    add_executable(mge_c_tests
        c/test_c_main.c
        c/test_c_lifecycle.c
        c/test_c_ecs.c
        c/test_c_input.c
        c/test_c_audio.c
        c/test_c_voxel.c
        c/test_c_safety.c
    )
    set_target_properties(mge_c_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
    target_include_directories(mge_c_tests PRIVATE ${miniGameEngine_SOURCE_DIR}/src)
    target_link_libraries(mge_c_tests PRIVATE mge::c)
    mge_set_warnings(mge_c_tests)

    add_test(NAME "c:lifecycle: headless lifecycle and timers" COMMAND mge_c_tests lifecycle)
    add_test(NAME "c:ecs: dynamic component registration and systems" COMMAND mge_c_tests ecs)
    add_test(NAME "c:input: action mapping and queries" COMMAND mge_c_tests input)
    add_test(NAME "c:audio: offline render and sfx generation" COMMAND mge_c_tests audio)
    add_test(NAME "c:voxel: world, c generator and raycast" COMMAND mge_c_tests voxel)
    add_test(NAME "c:safety: safe gpu resource release on shutdown" COMMAND mge_c_tests safety)
endif()
