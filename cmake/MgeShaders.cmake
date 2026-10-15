# Shaders: GLSL (Vulkan) → SPIR-V en tiempo de compilación, embebido en headers C++ (#29).
# Ningún programa lee archivos de shader en tiempo de ejecución.
#
#   mge_add_shaders(<target> SHADERS <a.vert> <b.frag> ...)
#
# Por cada shader genera <build>/.../generated/shaders/<nombre>_<etapa>.h con la constante
# mge::shaders::k<Nombre><Etapa> (un mge::ShaderCode: bytes SPIR-V + etapa). Incluir con
# #include <shaders/triangle_vert.h>. La etapa sale de la extensión: .vert, .frag, .comp.
# Editar un shader lo recompila (y a quien incluye su header).
#
# Compilador: glslang del sistema (Fedora: glslang, Ubuntu: glslang-tools) o, si no está,
# glslang compilado desde el código fuente (FetchContent, versión fija), como SDL.
include_guard(GLOBAL)

find_program(MGE_GLSLANG_EXECUTABLE NAMES glslang glslangValidator
    DOC "Compilador GLSL → SPIR-V (glslang)")
if(MGE_GLSLANG_EXECUTABLE)
    message(STATUS "glslang: usando el del sistema (${MGE_GLSLANG_EXECUTABLE})")
    set(MGE_GLSLANG_COMMAND "${MGE_GLSLANG_EXECUTABLE}")
    set(MGE_GLSLANG_TARGET "")
else()
    message(STATUS "glslang: no está instalado, se compila desde el código fuente")
    include(FetchContent)
    # Solo el compilador de línea de comandos: sin optimizador (SPIRV-Tools), HLSL, tests ni install
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
endif()

set(MGE_EMBED_SPIRV_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/MgeEmbedSpirv.cmake")

# Todos los headers de shaders generados: scripts/lint.sh lo compila antes de clang-tidy
# (que necesita los headers aunque no se haya compilado nada más)
add_custom_target(mge_generated_shaders)

function(mge_add_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SHADERS")
    if(NOT ARG_SHADERS)
        message(FATAL_ERROR "mge_add_shaders(${target}): falta SHADERS")
    endif()
    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    set(headers "")

    foreach(shader IN LISTS ARG_SHADERS)
        get_filename_component(source "${shader}" ABSOLUTE)
        get_filename_component(file_name "${shader}" NAME)
        if(NOT file_name MATCHES "\\.(vert|frag|comp)$")
            message(FATAL_ERROR "mge_add_shaders(${target}): ${file_name} debe terminar en .vert, .frag o .comp")
        endif()
        string(REGEX REPLACE "[^A-Za-z0-9]" "_" stem "${file_name}") # triangle.vert → triangle_vert
        set(spirv "${out_dir}/${file_name}.spv")
        set(header "${out_dir}/${stem}.h")
        add_custom_command(
            OUTPUT "${header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
            COMMAND ${MGE_GLSLANG_COMMAND} -V --target-env vulkan1.0 --quiet -o "${spirv}" "${source}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${spirv} -DOUTPUT=${header} -DSOURCE_NAME=${file_name}
                    -P "${MGE_EMBED_SPIRV_SCRIPT}"
            DEPENDS "${source}" "${MGE_EMBED_SPIRV_SCRIPT}" ${MGE_GLSLANG_TARGET}
            COMMENT "Shader ${file_name} → SPIR-V embebido"
            VERBATIM)
        target_sources(${target} PRIVATE "${header}")
        list(APPEND headers "${header}")
    endforeach()
    add_custom_target(${target}_shaders DEPENDS ${headers})
    add_dependencies(mge_generated_shaders ${target}_shaders)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endfunction()
