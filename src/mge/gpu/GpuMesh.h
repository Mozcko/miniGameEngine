#pragma once

#include <mge/gpu/Handles.h>
#include <mge/procgen/MeshData.h>

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Vértice entrelazado que sube a la GPU: todos los atributos de MeshData (también el color, que el
    // renderer OpenGL nunca enviaba, KI#13). Locations del shader: 0 posición, 1 normal, 2 color, 3 uv.
    struct GpuVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 color;
        glm::vec2 uv;
    };
    static_assert(sizeof(GpuVertex) == 48, "GpuVertex debe ser compacto (sin relleno)");

    // MeshData (arreglos separados) → vértices entrelazados
    std::vector<GpuVertex> interleave(const MeshData& mesh);

    // Descripción del layout de GpuVertex para crear pipelines (buffer en el slot 0)
    SDL_GPUVertexBufferDescription gpuVertexBufferDescription();
    std::array<SDL_GPUVertexAttribute, 4> gpuVertexAttributes();

    // Malla en la GPU: vértices + índices (uint32, triángulos)
    struct GpuMesh {
        GpuBuffer vertices;
        GpuBuffer indices;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;

        bool empty() const { return indexCount == 0; }
    };
}
