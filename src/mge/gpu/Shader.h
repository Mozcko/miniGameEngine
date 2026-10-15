#pragma once

#include <cstddef>
#include <cstdint>

namespace mge {

    enum class ShaderStage {
        Vertex,
        Fragment,
        Compute
    };

    // Shader compilado (SPIR-V) embebido en el binario: lo generan mge_add_shaders y
    // cmake/MgeEmbedSpirv.cmake como mge::shaders::k<Nombre><Etapa>.
    struct ShaderCode {
        const std::uint8_t* code = nullptr;
        std::size_t size = 0;
        ShaderStage stage = ShaderStage::Vertex;
    };

    // Cuántos recursos de cada tipo usa un shader (SDL GPU no los deduce del SPIR-V).
    // Convención de bindings para GLSL → SPIR-V (la que exige SDL GPU):
    //   vertex:   set 0 = texturas con sampler, storage textures, storage buffers; set 1 = uniform buffers
    //   fragment: set 2 = texturas con sampler, storage textures, storage buffers; set 3 = uniform buffers
    // Dentro de cada set los bindings van en ese orden, desde 0.
    struct ShaderResources {
        std::uint32_t samplers = 0;
        std::uint32_t storageTextures = 0;
        std::uint32_t storageBuffers = 0;
        std::uint32_t uniformBuffers = 0;
    };
}
