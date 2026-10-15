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

# --- Ejecutable de tests ---
add_executable(mge_tests
    test_main.cpp
    test_block.cpp
    test_chunk.cpp
)
target_link_libraries(mge_tests PRIVATE sandbox_world doctest_header)
mge_set_warnings(mge_tests)

# Registra cada TEST_CASE como un test de CTest
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(mge_tests)
