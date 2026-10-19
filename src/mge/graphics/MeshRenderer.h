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

    // Estadísticas del último frame, por pasada (índice = MeshPass)
    struct RenderStats {
        std::array<std::uint32_t, kMeshPassCount> drawCalls{};
        std::array<std::uint64_t, kMeshPassCount> triangles{};
        std::uint32_t totalDrawCalls() const { return drawCalls[0] + drawCalls[1] + drawCalls[2]; }
        std::uint64_t totalTriangles() const { return triangles[0] + triangles[1] + triangles[2]; }
    };

    // Dibuja mallas (GpuMesh) con una textura × color de vértice, cámara y profundidad. Back-face culling
    // con caras antihorarias de frente (como MeshData). Uso por frame:
    //   renderer.begin(cmd, target, w, h, cielo); renderer.setCamera(viewProj);
    //   renderer.draw(malla, model, &textura[, MeshPass::Transparent]) ...; renderer.end();
    // Con draw() el orden lo pone quien llama (opaco, cutout, transparente); con submit(cola) lo ordena
    // la RenderQueue.
    class MeshRenderer {
    public:
        MeshRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat);

        bool valid() const {
            return m_opaque && m_cutout && m_transparent && m_lines && m_sampler && m_white;
        }

        // Abre el render pass sobre target (limpia color y profundidad). El buffer de profundidad se
        // (re)crea si cambia el tamaño. false = no se pudo (no dibujar)
        bool begin(CommandBuffer& cmd, SDL_GPUTexture* target, std::uint32_t width, std::uint32_t height,
                   const glm::vec4& clearColor);
        // Proyección con Z en [0, 1] (Camera del ECS: cameraMatrices, o glm::perspectiveRH_ZO)
        // cameraPosition: para la niebla (distancia a la cámara)
        void setCamera(const glm::mat4& viewProjection, const glm::vec3& cameraPosition = glm::vec3(0.0f)) {
            m_viewProjection = viewProjection;
            m_cameraPosition = cameraPosition;
            m_lightingDirty = true;
        }
        // Luz y niebla (#33). Por defecto Lighting::unlit(): la textura × color de vértice tal cual
        void setLighting(const Lighting& lighting) {
            m_lighting = lighting;
            m_lightingDirty = true;
        }
        const Lighting& lighting() const { return m_lighting; }
        // texture == nullptr → blanca (solo el color de los vértices)
        void draw(const GpuMesh& mesh, const glm::mat4& model, const Texture2D* texture = nullptr,
                  MeshPass pass = MeshPass::Opaque, const MeshMaterial& material = {});
        // Dibuja una cola ya ordenada (RenderQueue::sort)
        void submit(const RenderQueue& queue);
        // Lista de líneas (pares de GpuVertex, posición + color) con prueba de profundidad sin escribirla.
        // Lo usa DebugDraw::flush
        void drawLines(const GpuBuffer& vertices, std::uint32_t vertexCount);
        void end();

        // Del frame actual / último
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
