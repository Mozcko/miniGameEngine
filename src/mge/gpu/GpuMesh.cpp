#include <mge/gpu/GpuMesh.h>

#include <cstddef>

namespace mge {

    std::vector<GpuVertex> interleave(const MeshData& mesh) {
        std::vector<GpuVertex> vertices(mesh.vertexCount());
        for (std::size_t i = 0; i < vertices.size(); i++)
            vertices[i] = {mesh.positions[i], mesh.normals[i], mesh.colors[i], mesh.uvs[i]};
        return vertices;
    }

    SDL_GPUVertexBufferDescription gpuVertexBufferDescription() {
        SDL_GPUVertexBufferDescription desc{};
        desc.slot = 0;
        desc.pitch = sizeof(GpuVertex);
        desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        return desc;
    }

    std::array<SDL_GPUVertexAttribute, 4> gpuVertexAttributes() {
        std::array<SDL_GPUVertexAttribute, 4> attributes{};
        attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                         static_cast<Uint32>(offsetof(GpuVertex, position))};
        attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                         static_cast<Uint32>(offsetof(GpuVertex, normal))};
        attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                         static_cast<Uint32>(offsetof(GpuVertex, color))};
        attributes[3] = {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                         static_cast<Uint32>(offsetof(GpuVertex, uv))};
        return attributes;
    }
}
