# Shaders: GLSL (Vulkan) → SPIR-V at build time, embedded in C++ headers (#29).
# No program reads shader files at runtime.
#
#   mge_add_shaders(<target> SHADERS <a.vert> <b.frag> ...)
#
# For each shader, generates <build>/.../generated/shaders/<name>_<stage>.h with the constant
# mge::shaders::k<Name><Stage> (an mge::ShaderCode: SPIR-V bytes + stage). Include with
# #include <shaders/triangle_vert.h>. The stage comes from the extension: .vert, .frag, .comp.
# Editing a shader recompiles it (and anything that includes its header).
#
# Compiler: system glslang (Fedora: glslang, Ubuntu: glslang-tools) or, if missing,
# glslang built from source (FetchContent, pinned version), just like SDL.
include_guard(GLOBAL)

find_program(MGE_GLSLANG_EXECUTABLE NAMES glslang glslangValidator
    DOC "GLSL → SPIR-V compiler (glslang)")
if(MGE_GLSLANG_EXECUTABLE)
    message(STATUS "glslang: using system executable (${MGE_GLSLANG_EXECUTABLE})")
    set(MGE_GLSLANG_COMMAND "${MGE_GLSLANG_EXECUTABLE}")
    set(MGE_GLSLANG_TARGET "")
else()
    if(NOT MGE_FROM_INSTALLED_PACKAGE)
        message(STATUS "glslang: not installed, building from source")
        include(FetchContent)
        # Command-line compiler only: no optimizer (SPIRV-Tools), HLSL, tests, or install
        set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
        set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
        set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
        set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
        set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
        FetchContent_Declare(glslang
            URL https://github.com/KhronosGroup/glslang/archive/refs/tags/16.2.0.tar.gz
            URL_HASH SHA256=01985335785c97906a91afe3cb5ee015997696181ec6c125bab5555602ba08e2
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(glslang)
        set(MGE_GLSLANG_COMMAND "$<TARGET_FILE:glslang-standalone>")
        set(MGE_GLSLANG_TARGET glslang-standalone)
    else()
        message(FATAL_ERROR "glslang not found on the system. mge_add_shaders requires glslang or glslangValidator.")
    endif()
endif()

set(MGE_EMBED_SPIRV_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/MgeEmbedSpirv.cmake")

# MSL shader generation (for Metal on macOS): SPIR-V → MSL via spirv-cross.
# Enabled by default on Apple platforms; off elsewhere so Linux/Windows need no extra tool.
option(MGE_SHADERS_MSL "Generate and embed MSL shaders alongside SPIR-V (for Metal on macOS)" ${APPLE})
if(MGE_SHADERS_MSL)
    find_program(MGE_SPIRV_CROSS_EXECUTABLE NAMES spirv-cross
        DOC "SPIR-V → MSL cross-compiler (SPIRV-Cross)")
    if(MGE_SPIRV_CROSS_EXECUTABLE)
        message(STATUS "spirv-cross: using ${MGE_SPIRV_CROSS_EXECUTABLE} (MSL shaders enabled)")
    else()
        message(FATAL_ERROR "MGE_SHADERS_MSL=ON requires spirv-cross (e.g. 'brew install spirv-cross'). Pass -DMGE_SHADERS_MSL=OFF to build SPIR-V only.")
    endif()
endif()

# Engine shader include directory (.glsl)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../include/mge/shaders/include")
    get_filename_component(MGE_DEFAULT_SHADER_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../include/mge/shaders/include" ABSOLUTE)
elseif(DEFINED miniGameEngine_SOURCE_DIR AND EXISTS "${miniGameEngine_SOURCE_DIR}/src/mge/graphics/shaders/include")
    set(MGE_DEFAULT_SHADER_INCLUDE_DIR "${miniGameEngine_SOURCE_DIR}/src/mge/graphics/shaders/include")
elseif(DEFINED mge_SOURCE_DIR AND EXISTS "${mge_SOURCE_DIR}/src/mge/graphics/shaders/include")
    set(MGE_DEFAULT_SHADER_INCLUDE_DIR "${mge_SOURCE_DIR}/src/mge/graphics/shaders/include")
elseif(EXISTS "${PROJECT_SOURCE_DIR}/src/mge/graphics/shaders/include")
    set(MGE_DEFAULT_SHADER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src/mge/graphics/shaders/include")
endif()

# All generated shader headers: scripts/lint.sh builds this before clang-tidy
# (which needs the headers even if nothing else was built)
if(NOT TARGET mge_generated_shaders)
    add_custom_target(mge_generated_shaders)
endif()

function(mge_add_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SHADERS;INCLUDE_DIRS")
    if(NOT ARG_SHADERS)
        message(FATAL_ERROR "mge_add_shaders(${target}): missing SHADERS")
    endif()
    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    set(headers "")

    set(include_args "")
    if(MGE_DEFAULT_SHADER_INCLUDE_DIR)
        list(APPEND include_args "-I${MGE_DEFAULT_SHADER_INCLUDE_DIR}")
    endif()
    foreach(inc_dir IN LISTS ARG_INCLUDE_DIRS)
        get_filename_component(abs_inc "${inc_dir}" ABSOLUTE)
        list(APPEND include_args "-I${abs_inc}")
    endforeach()

    foreach(shader IN LISTS ARG_SHADERS)
        get_filename_component(source "${shader}" ABSOLUTE)
        get_filename_component(file_name "${shader}" NAME)
        if(NOT file_name MATCHES "\\.(vert|frag|comp)$")
            message(FATAL_ERROR "mge_add_shaders(${target}): ${file_name} must end in .vert, .frag or .comp")
        endif()
        if(file_name MATCHES "\\.vert$")
            set(stage_flag "-S" "vert")
        elseif(file_name MATCHES "\\.frag$")
            set(stage_flag "-S" "frag")
        else()
            set(stage_flag "-S" "comp")
        endif()
        string(REGEX REPLACE "[^A-Za-z0-9]" "_" stem "${file_name}") # triangle.vert → triangle_vert
        set(spirv "${out_dir}/${file_name}.spv")
        set(header "${out_dir}/${stem}.h")
        set(depfile "${spirv}.d")
        if(MGE_SHADERS_MSL)
            set(msl "${out_dir}/${file_name}.msl")
            add_custom_command(
                OUTPUT "${spirv}" "${msl}" "${header}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
                COMMAND ${MGE_GLSLANG_COMMAND} -V ${stage_flag} --target-env vulkan1.0 --quiet
                        ${include_args} --depfile "${depfile}" -o "${spirv}" "${source}"
                COMMAND ${MGE_SPIRV_CROSS_EXECUTABLE} "${spirv}" --msl --msl-version 20100 --output "${msl}"
                COMMAND ${CMAKE_COMMAND} -DINPUT=${spirv} -DMSL_INPUT=${msl} -DOUTPUT=${header}
                        -DSOURCE_NAME=${file_name} -P "${MGE_EMBED_SPIRV_SCRIPT}"
                DEPFILE "${depfile}"
                DEPENDS "${source}" "${MGE_EMBED_SPIRV_SCRIPT}" ${MGE_GLSLANG_TARGET}
                COMMENT "Shader ${file_name} → embedded SPIR-V + MSL"
                VERBATIM)
        else()
            add_custom_command(
                OUTPUT "${spirv}" "${header}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
                COMMAND ${MGE_GLSLANG_COMMAND} -V ${stage_flag} --target-env vulkan1.0 --quiet
                        ${include_args} --depfile "${depfile}" -o "${spirv}" "${source}"
                COMMAND ${CMAKE_COMMAND} -DINPUT=${spirv} -DOUTPUT=${header} -DSOURCE_NAME=${file_name}
                        -P "${MGE_EMBED_SPIRV_SCRIPT}"
                DEPFILE "${depfile}"
                DEPENDS "${source}" "${MGE_EMBED_SPIRV_SCRIPT}" ${MGE_GLSLANG_TARGET}
                COMMENT "Shader ${file_name} → embedded SPIR-V"
                VERBATIM)
        endif()
        target_sources(${target} PRIVATE "${header}")
        list(APPEND headers "${header}")
    endforeach()
    add_custom_target(${target}_shaders DEPENDS ${headers})
    add_dependencies(mge_generated_shaders ${target}_shaders)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endfunction()
