#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class MeshRenderer;

    // Debug lines built on the CPU (#38): testable without GPU. Pairs of vertices (line list).
    class DebugLines {
    public:
        void clear() { m_vertices.clear(); }
        void line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
        void aabb(const Aabb& box, const glm::vec4& color); // 12 edges
        void sphere(const glm::vec3& center, float radius, const glm::vec4& color,
                    int segments = 24);                 // 3 circles
        void axes(const glm::vec3& origin, float size); // X red, Y green, Z blue
        // The 12 edges of a camera's visible volume (Z in [0, 1]: engine projections)
        void frustum(const glm::mat4& viewProjection, const glm::vec4& color);

        const std::vector<GpuVertex>& vertices() const { return m_vertices; }
        std::size_t lineCount() const { return m_vertices.size() / 2; }

    private:
        std::vector<GpuVertex> m_vertices;
    };

    // DebugLines + its GPU buffer. Per-frame usage: build the lines, and inside the MeshRenderer render pass
    // (after submit) call flush(renderer). Lines are depth-tested (occluded by the scene) without writing
    // depth.
    class DebugDraw : public DebugLines {
    public:
        explicit DebugDraw(GpuDevice& device) : m_device(device) {}
        void flush(MeshRenderer& renderer);

    private:
        GpuDevice& m_device;
        GpuBuffer m_buffer;
        std::uint32_t m_capacity = 0; // Vertices
    };
}
