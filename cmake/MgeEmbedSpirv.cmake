# cmake -DINPUT=<x.spv> -DOUTPUT=<x.h> -DSOURCE_NAME=<x.vert> -P MgeEmbedSpirv.cmake
# Convierte un binario SPIR-V en un header con un mge::ShaderCode (lo usa mge_add_shaders).
file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR byte_count "${hex_length} / 2")

# Bytes como "0x03, 0x02, ..." con un salto de línea cada 16
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
string(REPEAT "0x[0-9a-f][0-9a-f], " 16 sixteen) # El regex de CMake no tiene {n}
string(REGEX REPLACE "(${sixteen})" "\\1\n        " bytes "${bytes}")

# Nombre: "test_triangle.vert" → kTestTriangleVert
string(REGEX REPLACE "[^A-Za-z0-9]+" ";" parts "${SOURCE_NAME}")
set(name "")
foreach(part IN LISTS parts)
    string(SUBSTRING "${part}" 0 1 first)
    string(SUBSTRING "${part}" 1 -1 rest)
    string(TOUPPER "${first}" first)
    string(APPEND name "${first}${rest}")
endforeach()

if(SOURCE_NAME MATCHES "\\.vert$")
    set(stage "Vertex")
elseif(SOURCE_NAME MATCHES "\\.frag$")
    set(stage "Fragment")
else()
    set(stage "Compute")
endif()

file(WRITE "${OUTPUT}" "// Generado desde ${SOURCE_NAME} por cmake/MgeEmbedSpirv.cmake: no editar.
#pragma once

#include <mge/gpu/Shader.h>

#include <cstdint>

namespace mge::shaders {
    alignas(4) inline constexpr std::uint8_t k${name}Spirv[] = {
        ${bytes}
    };
    inline constexpr ShaderCode k${name}{k${name}Spirv, ${byte_count}, ShaderStage::${stage}};
}
")
