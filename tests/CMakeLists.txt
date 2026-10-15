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
endif()

# --- Tests del sandbox (necesitan los módulos del motor) ---
if(MGE_BUILD_MODULES)
    add_executable(game_tests
        test_main.cpp
        game/test_block.cpp
        game/test_chunk.cpp
    )
    target_link_libraries(game_tests PRIVATE sandbox_world doctest_header)
    mge_set_warnings(game_tests)
    doctest_discover_tests(game_tests TEST_PREFIX "game: ")
endif()
