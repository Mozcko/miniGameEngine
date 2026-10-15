# --- doctest (solo para desarrollo, nunca se enlaza al juego) ---
include(FetchContent)
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # Solo necesitamos el header y scripts/cmake/doctest.cmake: SOURCE_SUBDIR apunta a una
    # carpeta sin CMakeLists para que no se ejecute el CMakeLists de doctest (pide CMake 3.0,
    # que CMake 4 ya no acepta).
    SOURCE_SUBDIR do-not-configure
)
FetchContent_MakeAvailable(doctest)

add_library(doctest_header INTERFACE)
target_include_directories(doctest_header SYSTEM INTERFACE ${doctest_SOURCE_DIR})

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

# --- Tests del core: solo enlazan mge_core (sin SDL ni GPU) ---
add_executable(mge_core_tests
    test_main.cpp
    core/test_application.cpp
    core/test_assets.cpp
    core/test_ecs.cpp
    core/test_geometry.cpp
    core/test_input.cpp
    core/test_log.cpp
    core/test_random.cpp
    core/test_log_stripped.cpp
    core/test_time.cpp
    core/test_umbrella.cpp
)
target_link_libraries(mge_core_tests PRIVATE mge_core doctest_header)
mge_set_warnings(mge_core_tests)
doctest_discover_tests(mge_core_tests TEST_PREFIX "core: ")

# --- Tests de los módulos headless (sin SDL ni GPU): siempre ---
add_executable(mge_procgen_tests
    test_main.cpp
    procgen/test_meshdata.cpp
    procgen/test_primitives.cpp
    procgen/test_mesh_ops.cpp
    procgen/test_noise.cpp
    procgen/test_sdf.cpp
)
target_link_libraries(mge_procgen_tests PRIVATE mge_procgen doctest_header)
mge_set_warnings(mge_procgen_tests)
doctest_discover_tests(mge_procgen_tests TEST_PREFIX "procgen: ")

add_executable(mge_voxel_tests
    test_main.cpp
    voxel/test_block_registry.cpp
    voxel/test_chunk_coords.cpp
    voxel/test_chunk_mesher.cpp
    voxel/test_voxel_world.cpp
)
target_link_libraries(mge_voxel_tests PRIVATE mge_voxel doctest_header)
mge_set_warnings(mge_voxel_tests)
doctest_discover_tests(mge_voxel_tests TEST_PREFIX "voxel: ")

# --- Tests de los módulos (necesitan SDL; no abren ventana) ---
if(MGE_BUILD_MODULES)
    add_executable(mge_platform_tests
        test_main.cpp
        platform/test_sdl_input.cpp
        platform/test_umbrella_platform.cpp
    )
    target_link_libraries(mge_platform_tests PRIVATE mge_platform doctest_header)
    mge_set_warnings(mge_platform_tests)
    doctest_discover_tests(mge_platform_tests TEST_PREFIX "platform: ")

    # GPU (SDL3 GPU, Vulkan). Sin ventana: dibujan en texturas. Sin GPU se saltean (MGE_REQUIRE_GPU=1 → fallan).
    # Con sanitizers se apaga LeakSanitizer: los drivers de Mesa pierden memoria que LSan no puede atribuir.
    add_executable(mge_gpu_tests
        test_main.cpp
        gpu/test_gpu_device.cpp
        gpu/test_gpu_shaders.cpp
        gpu/test_gpu_swapchain.cpp
    )
    target_link_libraries(mge_gpu_tests PRIVATE mge_gpu doctest_header)
    mge_add_shaders(mge_gpu_tests SHADERS gpu/shaders/test_triangle.vert gpu/shaders/test_triangle.frag)

    # Render: escenas comparadas con imágenes de referencia (tests/render/golden). MGE_UPDATE_GOLDEN=1 las reescribe
    add_executable(render_tests
        test_main.cpp
        render/RenderTest.h render/RenderTest.cpp
        render/test_render_scenes.cpp
    )
    target_link_libraries(render_tests PRIVATE mge_gpu doctest_header)
    target_compile_definitions(render_tests PRIVATE
        MGE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/render/golden"
        MGE_RENDER_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/render_output")
    mge_set_warnings(render_tests)
    mge_add_shaders(render_tests SHADERS
        render/shaders/vertex_colors.vert render/shaders/vertex_colors.frag
        render/shaders/lit_mesh.vert render/shaders/lit_mesh.frag)
    doctest_discover_tests(render_tests TEST_PREFIX "render: "
        PROPERTIES LABELS gpu ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
    mge_set_warnings(mge_gpu_tests)
    doctest_discover_tests(mge_gpu_tests TEST_PREFIX "gpu: "
        PROPERTIES LABELS gpu ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()

# --- Tests del sandbox (necesitan los módulos del motor) ---
if(MGE_BUILD_MODULES)
    add_executable(game_tests
        test_main.cpp
        game/test_sandbox_blocks.cpp
        game/test_sandbox_terrain.cpp
        game/test_camera.cpp
    )
    target_link_libraries(game_tests PRIVATE sandbox_world mge_render doctest_header)
    mge_set_warnings(game_tests)
    doctest_discover_tests(game_tests TEST_PREFIX "game: ")
endif()
