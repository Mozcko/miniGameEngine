#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/graphics/Lighting.h>
#include <mge/graphics/RenderQueue.h>
#include <mge/graphics/Texture2D.h>

#include <array>

#include <cstdint>

#include <glm/glm.hpp>

namespace mge {

    // Stats from the last frame, per pass (index = MeshPass)
    struct RenderStats {
        std::array<std::uint32_t, kMeshPassCount> drawCalls{};
        std::array<std::uint64_t, kMeshPassCount> triangles{};
        std::uint32_t totalDrawCalls() const { return drawCalls[0] + drawCalls[1] + drawCalls[2]; }
        std::uint64_t totalTriangles() const { return triangles[0] + triangles[1] + triangles[2]; }
    };

    // Draws meshes (GpuMesh) with texture × vertex color, camera, and depth. Back-face culling
    // with counter-clockwise front faces (like MeshData). Per-frame usage:
    //   renderer.begin(cmd, target, w, h, skyColor); renderer.setCamera(viewProj);
    //   renderer.draw(mesh, model, &texture[, MeshPass::Transparent]) ...; renderer.end();
    // With draw(), the caller controls order (opaque, cutout, transparent); with submit(queue),
    // RenderQueue sorts them.
    class MeshRenderer {
    public:
        MeshRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat);

        bool valid() const {
            return m_opaque && m_cutout && m_transparent && m_lines && m_sampler && m_white;
        }

        // Begins the render pass on target (clears color and depth). The depth buffer is
        // (re)created if the size changes. false = failed (do not draw)
        bool begin(CommandBuffer& cmd, SDL_GPUTexture* target, std::uint32_t width, std::uint32_t height,
                   const glm::vec4& clearColor);
        // Projection with Z in [0, 1] (ECS Camera: cameraMatrices, or glm::perspectiveRH_ZO)
        // cameraPosition: used for fog (distance to camera)
        void setCamera(const glm::mat4& viewProjection, const glm::vec3& cameraPosition = glm::vec3(0.0f)) {
            m_viewProjection = viewProjection;
            m_cameraPosition = cameraPosition;
            m_lightingDirty = true;
        }
        // Lighting and fog (#33). Defaults to Lighting::unlit(): texture × vertex color as-is
        void setLighting(const Lighting& lighting) {
            m_lighting = lighting;
            m_lightingDirty = true;
        }
        const Lighting& lighting() const { return m_lighting; }
        // texture == nullptr → white (vertex color only)
        void draw(const GpuMesh& mesh, const glm::mat4& model, const Texture2D* texture = nullptr,
                  MeshPass pass = MeshPass::Opaque, const MeshMaterial& material = {});
        // Draws an already sorted queue (RenderQueue::sort)
        void submit(const RenderQueue& queue);
        // Line list (pairs of GpuVertex, position + color) with depth testing and no depth write.
        // Used by DebugDraw::flush
        void drawLines(const GpuBuffer& vertices, std::uint32_t vertexCount);
        void end();

        // Active render pass and command buffer for complementary passes (SkyRenderer)
        SDL_GPURenderPass* currentPass() const { return m_pass.get(); }
        CommandBuffer* currentCommandBuffer() const { return m_cmd; }
        void resetBoundPipeline() {
            m_boundPipeline = nullptr;
            m_lightingDirty = true;
        }

        // From the current / last frame
        const RenderStats& stats() const { return m_stats; }
        std::uint32_t drawCalls() const { return m_stats.totalDrawCalls(); }
        std::uint64_t triangles() const { return m_stats.totalTriangles(); }

    private:
        GpuDevice& m_device;
        GpuGraphicsPipeline m_opaque;
        GpuGraphicsPipeline m_cutout;
        GpuGraphicsPipeline m_transparent;
        GpuGraphicsPipeline m_lines;
        GpuSampler m_sampler;
        Texture2D m_white;
        GpuTexture m_depth;
        std::uint32_t m_depthWidth = 0;
        std::uint32_t m_depthHeight = 0;

        CommandBuffer* m_cmd = nullptr;
        RenderPass m_pass;
        const SDL_GPUGraphicsPipeline* m_boundPipeline = nullptr;
        glm::mat4 m_viewProjection = glm::mat4(1.0f);
        glm::vec3 m_cameraPosition = glm::vec3(0.0f);
        Lighting m_lighting = Lighting::unlit();
        bool m_lightingDirty = true;
        RenderStats m_stats;
    };
}
