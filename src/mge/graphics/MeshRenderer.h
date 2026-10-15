#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/graphics/Texture2D.h>

#include <cstdint>

#include <glm/glm.hpp>

namespace mge {

    // Cómo se dibuja una malla
    enum class MeshPass {
        Opaque,      // Prueba y escritura de profundidad, sin mezcla
        Transparent, // Mezcla alfa, prueba de profundidad SIN escritura (dibujar después de lo opaco)
    };

    // Dibuja mallas (GpuMesh) con una textura × color de vértice, cámara y profundidad. Back-face culling
    // con caras antihorarias de frente (como MeshData). Uso por frame:
    //   renderer.begin(cmd, target, w, h, cielo); renderer.setCamera(viewProj);
    //   renderer.draw(malla, model, &textura[, MeshPass::Transparent]) ...; renderer.end();
    // Lo opaco primero y lo transparente después (el orden lo pone quien llama; la cola llega en #31).
    class MeshRenderer {
    public:
        MeshRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat);

        bool valid() const { return m_opaque && m_transparent && m_sampler && m_white; }

        // Abre el render pass sobre target (limpia color y profundidad). El buffer de profundidad se
        // (re)crea si cambia el tamaño. false = no se pudo (no dibujar)
        bool begin(CommandBuffer& cmd, SDL_GPUTexture* target, std::uint32_t width, std::uint32_t height,
                   const glm::vec4& clearColor);
        // Proyección con Z en [0, 1] (glm::perspectiveRH_ZO o depthZeroToOne)
        void setCamera(const glm::mat4& viewProjection) { m_viewProjection = viewProjection; }
        // texture == nullptr → blanca (solo el color de los vértices)
        void draw(const GpuMesh& mesh, const glm::mat4& model, const Texture2D* texture = nullptr,
                  MeshPass pass = MeshPass::Opaque);
        void end();

        // Del último frame
        std::uint32_t drawCalls() const { return m_drawCalls; }
        std::uint64_t triangles() const { return m_triangles; }

    private:
        GpuDevice& m_device;
        GpuGraphicsPipeline m_opaque;
        GpuGraphicsPipeline m_transparent;
        GpuSampler m_sampler;
        Texture2D m_white;
        GpuTexture m_depth;
        std::uint32_t m_depthWidth = 0;
        std::uint32_t m_depthHeight = 0;

        CommandBuffer* m_cmd = nullptr;
        RenderPass m_pass;
        const SDL_GPUGraphicsPipeline* m_boundPipeline = nullptr;
        glm::mat4 m_viewProjection = glm::mat4(1.0f);
        std::uint32_t m_drawCalls = 0;
        std::uint64_t m_triangles = 0;
    };
}
