#pragma once

#include <mge/gpu/Handles.h>
#include <mge/procgen/MeshData.h>

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Interleaved vertex uploaded to the GPU: all MeshData attributes (including color, which the
    // OpenGL renderer never sent, KI#13). Shader locations: 0 position, 1 normal, 2 color, 3 uv.
    struct GpuVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 color;
        glm::vec2 uv;
    };
    static_assert(sizeof(GpuVertex) == 48, "GpuVertex must be tightly packed (no padding)");

    // MeshData (separate arrays) → interleaved vertices
    std::vector<GpuVertex> interleave(const MeshData& mesh);

    // GpuVertex layout description for pipeline creation (buffer in slot 0)
    SDL_GPUVertexBufferDescription gpuVertexBufferDescription();
    std::array<SDL_GPUVertexAttribute, 4> gpuVertexAttributes();

    // Mesh on the GPU: vertices + indices (uint32, triangles)
    struct GpuMesh {
        GpuBuffer vertices;
        GpuBuffer indices;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;

        bool empty() const { return indexCount == 0; }
    };
}
