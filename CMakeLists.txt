cmake_minimum_required(VERSION 3.24)
project(miniGameEngine VERSION 1.0.1 LANGUAGES C CXX)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json (clangd, clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release (with optimizations) if no build type was chosen
get_property(MGE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT MGE_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# --- Options ---
option(MGE_INSTALL "Install and export CMake packages for reusing the engine" OFF)
option(MGE_WARNINGS "Enable strict warnings in our code" ON)
option(MGE_WERROR "Treat warnings as errors" OFF)
option(MGE_BUILD_TESTS "Build unit tests (doctest + CTest)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer (Debug)" OFF)
option(MGE_TSAN "Build with ThreadSanitizer (Debug; incompatible with MGE_SANITIZERS)" OFF)
option(MGE_BUILD_BENCHMARKS "Build benchmarks (bench/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_EXAMPLES "Build examples (examples/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_SANDBOX "Build the sandbox (game/)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_BUILD_MODULES "Build SDL/GPU modules (platform, render, audio, app) and the game. OFF = mge_core only" ON)
option(MGE_SIZE_OPTIMIZE "Optimize binaries for minimum size (-Os, LTO/IPO, gc-sections)" OFF)
option(MGE_BUILD_CLI "Build the mge CLI tool (tools/mge)" ${PROJECT_IS_TOP_LEVEL})
option(MGE_PIC "Build libraries with position-independent code (fPIC)" OFF)
option(MGE_BUILD_C_SHARED "Build mge_c as a shared library" OFF)
option(MGE_BUILD_PYTHON "Build Python bindings" OFF)
if(MGE_BUILD_C_SHARED OR MGE_BUILD_PYTHON)
    set(MGE_PIC ON CACHE BOOL "Build libraries with position-independent code (fPIC)" FORCE)
endif()
if(MGE_PIC)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

option(MGE_SOURCE_ASSET_FALLBACK "Use the source tree assets/ folder as a last resort in assetPath()" ON)
if(MGE_INSTALL AND MGE_SOURCE_ASSET_FALLBACK)
    message(WARNING "MGE_INSTALL=ON and MGE_SOURCE_ASSET_FALLBACK=ON: installed libraries will contain an absolute path to this source tree; pass -DMGE_SOURCE_ASSET_FALLBACK=OFF for distributable packages")
endif()
option(MGE_PROFILING "Enable profiler instrumentation (MGE_PROFILE_SCOPE)" ON)
option(MGE_TRACY "Enable optional Tracy Profiler integration" OFF)

# Engine version (<mge/Version.h>)
configure_file(
    "${miniGameEngine_SOURCE_DIR}/src/mge/Version.h.in"
    "${miniGameEngine_BINARY_DIR}/generated/include/mge/Version.h"
    @ONLY
)

# --- Size optimization (MGE_SIZE_OPTIMIZE) ---
if(MGE_SIZE_OPTIMIZE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT MGE_IPO_SUPPORTED OUTPUT MGE_IPO_ERROR)
    if(MGE_IPO_SUPPORTED)
        message(STATUS "IPO/LTO supported and enabled")
    else()
        message(WARNING "IPO not supported: ${MGE_IPO_ERROR}")
    endif()
endif()

# --- ThreadSanitizer: data races (JobSystem and what runs on it) ---
if(MGE_TSAN)
    if(MGE_SANITIZERS)
        message(FATAL_ERROR "MGE_TSAN and MGE_SANITIZERS (ASan) cannot be combined")
    endif()
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# --- Sanitizers (entire build, including dependencies, so ASan sees all allocations) ---
if(MGE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# --- Size and warning flags (only for our targets, not for SDL/glslang) ---
function(mge_set_size_flags target)
    if(NOT MGE_SIZE_OPTIMIZE)
        return()
    endif()

    if(MGE_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()

    get_target_property(target_type ${target} TYPE)

    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Os -ffunction-sections -fdata-sections)
        if(target_type STREQUAL "STATIC_LIBRARY")
            target_compile_options(${target} PRIVATE -fvisibility=hidden)
        endif()
    endif()

    if(NOT MSVC AND NOT APPLE)
        if(target_type STREQUAL "EXECUTABLE")
            target_link_options(${target} PRIVATE -Wl,--gc-sections -s)
        endif()
    endif()
endfunction()

function(mge_set_warnings target)
    mge_set_size_flags(${target})
    if(MGE_PIC)
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    if(NOT MGE_WARNINGS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${MGE_WERROR}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow $<$<BOOL:${MGE_WERROR}>:-Werror>)
    endif()
endfunction()

# --- Module dependencies (core needs none) ---
if(MGE_BUILD_MODULES)
    # SDL3: use system package if installed (e.g. SDL3-devel on Fedora); otherwise build from
    # source (Ubuntu 24.04 CI, which does not package SDL3). Pinned version + hash.
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    # Optional X11 extensions not used by the engine (avoids requiring their -dev packages)
    set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz
        URL_HASH SHA256=c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        FIND_PACKAGE_ARGS 3.2 CONFIG
    )
    FetchContent_MakeAvailable(SDL3)
    if(SDL3_FOUND)
        message(STATUS "SDL3: using system installation (${SDL3_VERSION})")
    else()
        message(STATUS "SDL3: built from source (release-3.4.18)")
    endif()
    if(MGE_INSTALL AND NOT SDL3_FOUND)
        message(FATAL_ERROR "MGE_INSTALL=ON requires a system SDL3 installation (e.g. SDL3-devel or libsdl3-dev). SDL3 built via FetchContent is not exportable. Install SDL3 on the system or configure with -DMGE_INSTALL=OFF.")
    endif()
endif()

# --- Tracy Profiler (optional, -DMGE_TRACY=ON) ---
if(MGE_TRACY)
    include(FetchContent)
    FetchContent_Declare(tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.11.1.tar.gz
        URL_HASH SHA256=2c11ca816f2b756be2730f86b0092920419f3dabc7a7173829ffd897d91888a1
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(tracy)
endif()

# mge_add_game(): one call per game (see cmake/MgeGame.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MgeGame.cmake)
# mge_add_shaders(): GLSL → embedded SPIR-V (see cmake/MgeShaders.cmake). Only with modules (GPU)
if(MGE_BUILD_MODULES)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MgeShaders.cmake)
endif()

# CTest before subdirectories: games and examples also register tests
if(MGE_BUILD_TESTS)
    enable_testing()
endif()

if(MGE_SIZE_OPTIMIZE AND MGE_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# --- Engine (src/mge), games and examples ---
add_subdirectory(src/mge)
if(MGE_BUILD_MODULES AND MGE_BUILD_SANDBOX)
    add_subdirectory(game)
endif()
if(MGE_BUILD_EXAMPLES)
    # examples/ decides what to build based on MGE_BUILD_MODULES (headless ones always build)
    add_subdirectory(examples)
endif()

# --- Benchmarks ---
if(MGE_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# --- Tools (CLI) ---
if(MGE_BUILD_CLI)
    if(WIN32)
        set(MGE_BUILD_CLI OFF CACHE BOOL "Build the mge CLI tool (tools/mge)" FORCE)
        message(STATUS "mge CLI: disabled on Windows (tools/mge/Process.cpp requires POSIX)")
    else()
        add_subdirectory(tools/mge)
    endif()
endif()

# --- Unit tests ---
if(MGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Python bindings ---
if(MGE_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()


# --- CMake package (find_package(mge)) ---
if(MGE_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfigVersion.cmake"
        VERSION ${miniGameEngine_VERSION}
        COMPATIBILITY SameMajorVersion # semver: 1.x is compatible with 1.y (y ≥ x)
    )
    configure_package_config_file(
        "${miniGameEngine_SOURCE_DIR}/cmake/mgeConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfig.cmake"
        INSTALL_DESTINATION lib/cmake/mge
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/mgeConfigVersion.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeGame.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeShaders.cmake"
        "${miniGameEngine_SOURCE_DIR}/cmake/MgeEmbedSpirv.cmake"
        DESTINATION lib/cmake/mge
    )
    install(DIRECTORY templates/ DESTINATION share/mge/templates)
endif()
