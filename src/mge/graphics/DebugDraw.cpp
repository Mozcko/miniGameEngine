#include <mge/graphics/DebugDraw.h>

#include <mge/graphics/MeshRenderer.h>

#include <algorithm>
#include <cmath>

namespace mge {

    void DebugLines::line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
        m_vertices.push_back({a, glm::vec3(0.0f), color, glm::vec2(0.0f)});
        m_vertices.push_back({b, glm::vec3(0.0f), color, glm::vec2(0.0f)});
    }

    void DebugLines::aabb(const Aabb& box, const glm::vec4& color) {
        if (box.isEmpty()) return;
        const glm::vec3& lo = box.min;
        const glm::vec3& hi = box.max;
        const glm::vec3 c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z},
                                {lo.x, lo.y, hi.z}, {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
                                {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
        for (int i = 0; i < 4; i++) {
            line(c[i], c[(i + 1) % 4], color);         // Abajo
            line(c[i + 4], c[(i + 1) % 4 + 4], color); // Arriba
            line(c[i], c[i + 4], color);               // Verticales
        }
    }

    void DebugLines::sphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
        segments = std::max(segments, 3);
        for (int axis = 0; axis < 3; axis++) {
            for (int i = 0; i < segments; i++) {
                auto point = [&](int k) {
                    const float angle = 6.2831853f * float(k) / float(segments);
                    glm::vec3 p(0.0f);
                    p[(axis + 1) % 3] = std::cos(angle) * radius;
                    p[(axis + 2) % 3] = std::sin(angle) * radius;
                    return center + p;
                };
                line(point(i), point(i + 1), color);
            }
        }
    }

    void DebugLines::axes(const glm::vec3& origin, float size) {
        line(origin, origin + glm::vec3(size, 0, 0), {1, 0.2f, 0.2f, 1});
        line(origin, origin + glm::vec3(0, size, 0), {0.2f, 1, 0.2f, 1});
        line(origin, origin + glm::vec3(0, 0, size), {0.3f, 0.5f, 1, 1});
    }

    void DebugLines::frustum(const glm::mat4& viewProjection, const glm::vec4& color) {
        const glm::mat4 inverse = glm::inverse(viewProjection);
        glm::vec3 c[8];
        for (int i = 0; i < 8; i++) {
            const glm::vec4 ndc((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : 0.0f, 1.0f);
            const glm::vec4 world = inverse * ndc;
            c[i] = glm::vec3(world) / world.w;
        }
        // Cerca (0..3) y lejos (4..7): cuadrados + aristas que los unen
        const int quad[4] = {0, 1, 3, 2};
        for (int i = 0; i < 4; i++) {
            line(c[quad[i]], c[quad[(i + 1) % 4]], color);
            line(c[quad[i] + 4], c[quad[(i + 1) % 4] + 4], color);
            line(c[quad[i]], c[quad[i] + 4], color);
        }
    }

    void DebugDraw::flush(MeshRenderer& renderer) {
        const auto count = static_cast<std::uint32_t>(vertices().size());
        if (count == 0) return;
        if (count > m_capacity) {
            m_capacity = std::max(count, m_capacity * 2);
            m_buffer = m_device.createBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                             m_capacity * std::uint32_t(sizeof(GpuVertex)));
        }
        // cycle: si el frame anterior todavía usa el buffer, SDL da otro por detrás (sin pisarlo)
        if (!m_device.uploadToBuffer(m_buffer, vertices().data(), count * std::uint32_t(sizeof(GpuVertex)),
                                     true))
            return;
        renderer.drawLines(m_buffer, count);
    }
}
