#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class MeshRenderer;

    // Líneas de depuración armadas en la CPU (#38): se testean sin GPU. Pares de vértices (lista de líneas).
    class DebugLines {
    public:
        void clear() { m_vertices.clear(); }
        void line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
        void aabb(const Aabb& box, const glm::vec4& color); // 12 aristas
        void sphere(const glm::vec3& center, float radius, const glm::vec4& color,
                    int segments = 24);                 // 3 círculos
        void axes(const glm::vec3& origin, float size); // X rojo, Y verde, Z azul
        // Las 12 aristas del volumen visible de una cámara (Z en [0, 1]: proyecciones del motor)
        void frustum(const glm::mat4& viewProjection, const glm::vec4& color);

        const std::vector<GpuVertex>& vertices() const { return m_vertices; }
        std::size_t lineCount() const { return m_vertices.size() / 2; }

    private:
        std::vector<GpuVertex> m_vertices;
    };

    // DebugLines + su buffer en la GPU. Uso por frame: armar las líneas, y dentro del render pass del
    // MeshRenderer (después de submit) llamar a flush(renderer). Las líneas tienen prueba de profundidad
    // (se tapan con la escena) pero no la escriben.
    class DebugDraw : public DebugLines {
    public:
        explicit DebugDraw(GpuDevice& device) : m_device(device) {}
        void flush(MeshRenderer& renderer);

    private:
        GpuDevice& m_device;
        GpuBuffer m_buffer;
        std::uint32_t m_capacity = 0; // Vértices
    };
}
