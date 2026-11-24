#pragma once

#include <mge/gpu/GpuMesh.h>

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class Texture2D;

    // How a mesh is drawn (the enum order is the draw order)
    enum class MeshPass {
        Opaque,      // Depth test and write, no blending
        Cutout,      // Like Opaque, but discards pixels with alpha < 0.5 (leaves, plants)
        Transparent, // Alpha blending, depth test WITHOUT write (after opaque)
    };
    inline constexpr std::size_t kMeshPassCount = 3;

    // Procedural per-mesh material (#34): world-space brightness noise, for solid-color meshes
    // (untextured blocks) which with greedy meshing produce large flat faces
    struct MeshMaterial {
        float detailScale = 4.0f;    // Noise cells per world unit
        float detailStrength = 0.0f; // 0 = no detail; 0.1–0.3 = subtle
    };

    // A mesh to draw in the frame
    struct DrawItem {
        const GpuMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        const Texture2D* texture = nullptr; // nullptr = white
        MeshPass pass = MeshPass::Opaque;
        glm::vec3 center{0.0f}; // World-space center (for sorting)
        MeshMaterial material;
    };

    // Per-frame draw queue. sort() orders it for correct and fast rendering:
    //   1. by pass (opaque, cutout, transparent);
    //   2. opaque and cutout front-to-back (depth test rejects occluded fragments before shading);
    //   3. transparent back-to-front (alpha blending requires this order).
    // Sorting is per item center (e.g. a chunk); triangles within a mesh are not sorted.
    class RenderQueue {
    public:
        void add(const DrawItem& item);
        void clear() { m_items.clear(); }
        // depth = distance along the view direction: dot(center - cameraPosition, cameraForward)
        void sort(const glm::vec3& cameraPosition, const glm::vec3& cameraForward);

        const std::vector<DrawItem>& items() const { return m_items; }
        std::size_t size() const { return m_items.size(); }

    private:
        std::vector<DrawItem> m_items;
    };
}
