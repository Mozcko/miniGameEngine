# mge_add_game(): creates a game executable in a single call.
#
#   mge_add_game(<name>
#       SOURCES <files...>                 # required
#       [LIBRARIES <libs...>]              # extra game libraries (e.g. sandbox_world)
#       [MODULES platform gpu graphics audio]  # engine modules (default: all four)
#       [HEADLESS]                         # no modules: no window, GPU or SDL
#       [SMOKE_FRAMES <n>])                # registers a CTest smoke test that runs n frames
#
# Additionally: links mge_main (the engine's main()), applies project warnings, places the
# executable in the build dir root, and creates the run-<name> target (and `run` for MGE_MAIN_GAME).
set(MGE_MAIN_GAME "sandbox" CACHE STRING "Game executed by the 'run' target")

enable_testing()

if(NOT COMMAND mge_set_warnings)
    function(mge_set_warnings target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow)
        endif()
    endfunction()
endif()

function(mge_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "HEADLESS" "SMOKE_FRAMES" "SOURCES;LIBRARIES;MODULES")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "mge_add_game(${name}): missing SOURCES")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "mge_add_game(${name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE mge::main ${ARG_LIBRARIES})

    if(ARG_HEADLESS)
        if(ARG_MODULES)
            message(FATAL_ERROR "mge_add_game(${name}): HEADLESS does not allow MODULES")
        endif()
    else()
        if(NOT ARG_MODULES)
            set(ARG_MODULES platform gpu graphics audio)
        endif()
        foreach(module IN LISTS ARG_MODULES)
            if(NOT module MATCHES "^(platform|gpu|graphics|audio|physics|procgen|gltf|voxel|synth|core)$")
                message(FATAL_ERROR "mge_add_game(${name}): unknown module '${module}' (platform, gpu, graphics, audio, physics, procgen, gltf, voxel, synth, core)")
            endif()
            target_link_libraries(${name} PRIVATE mge::${module})
        endforeach()
    endif()

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    mge_set_warnings(${name})

    # Build and run in one step
    add_custom_target(run-${name} COMMAND ${name} DEPENDS ${name} USES_TERMINAL COMMENT "Running ${name}")
    if(DEFINED MGE_MAIN_GAME AND name STREQUAL MGE_MAIN_GAME AND NOT TARGET run)
        add_custom_target(run COMMAND ${name} DEPENDS ${name} USES_TERMINAL COMMENT "Running ${name}")
    endif()

    # Smoke test: runs n frames from another directory (proves it does not depend on cwd)
    if(ARG_SMOKE_FRAMES)
        set(environment "MGE_MAX_FRAMES=${ARG_SMOKE_FRAMES}")
        if(ARG_HEADLESS)
            set(test_name "smoke: ${name} headless ${ARG_SMOKE_FRAMES} frames")
            set(labels "")
        else()
            set(test_name "smoke: ${name} offscreen ${ARG_SMOKE_FRAMES} frames")
            # Offscreen window (SDL offscreen + EGL). With sanitizers, disable LeakSanitizer only:
            # Mesa's EGL driver on Ubuntu leaks memory that LSan cannot attribute (see #18).
            list(APPEND environment "SDL_VIDEODRIVER=offscreen" "SDL_AUDIODRIVER=dummy"
                 "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
            set(labels gpu)
        endif()
        set(smoke_cwd "${CMAKE_BINARY_DIR}/smoke-cwd")
        file(MAKE_DIRECTORY "${smoke_cwd}")
        add_test(NAME "${test_name}" COMMAND ${name} WORKING_DIRECTORY "${smoke_cwd}")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 120 ENVIRONMENT "${environment}")
        if(labels)
            # With Vulkan validation layers installed, a GPU API misuse fails the test
            set_tests_properties("${test_name}" PROPERTIES LABELS "${labels}" FAIL_REGULAR_EXPRESSION "Validation Error")
        endif()
    endif()
endfunction()

# mge_add_c_game(): creates a pure C (C99) game executable linked to mge::c (#161).
function(mge_add_c_game name)
    enable_language(C)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "HEADLESS" "SMOKE_FRAMES" "SOURCES;LIBRARIES;MODULES")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "mge_add_c_game(${name}): missing SOURCES")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "mge_add_c_game(${name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        LINKER_LANGUAGE CXX
    )
    target_link_libraries(${name} PRIVATE mge::c)
    if(ARG_LIBRARIES)
        target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
    endif()

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    mge_set_warnings(${name})

    add_custom_target(run-${name} COMMAND ${name} DEPENDS ${name} USES_TERMINAL COMMENT "Running ${name}")

    if(ARG_SMOKE_FRAMES)
        set(environment "MGE_MAX_FRAMES=${ARG_SMOKE_FRAMES}")
        if(ARG_HEADLESS)
            set(test_name "smoke: ${name} headless ${ARG_SMOKE_FRAMES} frames")
            set(labels "")
        else()
            set(test_name "smoke: ${name} offscreen ${ARG_SMOKE_FRAMES} frames")
            list(APPEND environment "SDL_VIDEODRIVER=offscreen" "SDL_AUDIODRIVER=dummy"
                 "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
            set(labels gpu)
        endif()
        set(smoke_cwd "${CMAKE_BINARY_DIR}/smoke-cwd")
        file(MAKE_DIRECTORY "${smoke_cwd}")
        add_test(NAME "${test_name}" COMMAND ${name} WORKING_DIRECTORY "${smoke_cwd}")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 120 ENVIRONMENT "${environment}")
        if(labels)
            set_tests_properties("${test_name}" PROPERTIES LABELS "${labels}" FAIL_REGULAR_EXPRESSION "Validation Error")
        endif()
    endif()
endfunction()
