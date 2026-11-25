# cmake -DINPUT=<x.spv> [-DMSL_INPUT=<x.msl>] -DOUTPUT=<x.h> -DSOURCE_NAME=<x.vert> -P MgeEmbedSpirv.cmake
# Converts a SPIR-V binary (and optional MSL source) into a header with an mge::ShaderCode
# (used by mge_add_shaders).
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    return()
endif()

file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR byte_count "${hex_length} / 2")

# Bytes as "0x03, 0x02, ..." with a line break every 16
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
string(REPEAT "0x[0-9a-f][0-9a-f], " 16 sixteen) # CMake regex does not support {n}
string(REGEX REPLACE "(${sixteen})" "\\1\n        " bytes "${bytes}")

# Name: "test_triangle.vert" → kTestTriangleVert
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

set(msl_decl "")
set(msl_init "")
if(DEFINED MSL_INPUT AND EXISTS "${MSL_INPUT}")
    # Remap [[buffer(N)]] indices in spirv-cross MSL output to match SDL3 GPU's Metal
    # binding order: uniform buffers (constant) first, then readonly storage buffers
    # (const device), then readwrite storage buffers (device).
    file(READ "${MSL_INPUT}" msl_text)
    string(REGEX MATCHALL "(const[ \t]+device|constant|device)[ \t]+[^,)\\n\\r]+\\[\\[buffer\\([0-9]+\\)\\]\\]"
        msl_buf_matches "${msl_text}")
    set(uniform_matches "")
    set(ro_matches "")
    set(rw_matches "")
    foreach(m IN LISTS msl_buf_matches)
        if(m MATCHES "^constant[ \t]")
            list(APPEND uniform_matches "${m}")
        elseif(m MATCHES "^const[ \t]+device[ \t]")
            list(APPEND ro_matches "${m}")
        else()
            list(APPEND rw_matches "${m}")
        endif()
    endforeach()

    set(ordered_matches ${uniform_matches} ${ro_matches} ${rw_matches})
    set(next_idx 0)
    foreach(m IN LISTS ordered_matches)
        string(REGEX REPLACE "\\[\\[buffer\\([0-9]+\\)\\]\\]" "[[__MGE_BUF_${next_idx}__]]" m_new "${m}")
        string(REPLACE "${m}" "${m_new}" msl_text "${msl_text}")
        math(EXPR next_idx "${next_idx} + 1")
    endforeach()
    string(REGEX REPLACE "\\[\\[__MGE_BUF_([0-9]+)__\\]\\]" "[[buffer(\\1)]]" msl_text "${msl_text}")
    file(WRITE "${MSL_INPUT}" "${msl_text}")

    file(READ "${MSL_INPUT}" msl_hex HEX)
    string(LENGTH "${msl_hex}" msl_hex_length)
    math(EXPR msl_byte_count "${msl_hex_length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " msl_bytes "${msl_hex}")
    string(REGEX REPLACE "(${sixteen})" "\\1\n        " msl_bytes "${msl_bytes}")
    set(msl_decl "\n    inline constexpr std::uint8_t k${name}Msl[] = {\n        ${msl_bytes}\n    };")
    set(msl_init ", k${name}Msl, ${msl_byte_count}")
endif()

file(WRITE "${OUTPUT}" "// Generated from ${SOURCE_NAME} by cmake/MgeEmbedSpirv.cmake: do not edit.
#pragma once

#include <mge/gpu/Shader.h>

#include <cstdint>

namespace mge::shaders {
    alignas(4) inline constexpr std::uint8_t k${name}Spirv[] = {
        ${bytes}
    };${msl_decl}
    inline constexpr ShaderCode k${name}{k${name}Spirv, ${byte_count}, ShaderStage::${stage}${msl_init}};
}
")
