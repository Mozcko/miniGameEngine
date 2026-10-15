#pragma once

#include <SDL3/SDL_gpu.h>

#include <glm/glm.hpp>

namespace mge {

    // Destino de color de un render pass: se limpia con clearColor (o se conserva si clear = false)
    struct ColorTarget {
        SDL_GPUTexture* texture = nullptr;
        glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        bool clear = true;
    };

    // Destino de profundidad: se limpia a clearDepth (1 = lejos)
    struct DepthTarget {
        SDL_GPUTexture* texture = nullptr;
        float clearDepth = 1.0f;
    };

    // Render pass abierto: termina al destruirse (RAII). Solo se puede mover.
    class RenderPass {
    public:
        RenderPass() = default;
        explicit RenderPass(SDL_GPURenderPass* pass) : m_pass(pass) {}
        ~RenderPass() { end(); }
        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&& other) noexcept;
        RenderPass& operator=(RenderPass&& other) noexcept;

        void end();
        SDL_GPURenderPass* get() const { return m_pass; }
        explicit operator bool() const { return m_pass != nullptr; }

    private:
        SDL_GPURenderPass* m_pass = nullptr;
    };

    // Copy pass abierto (subidas y descargas): termina al destruirse
    class CopyPass {
    public:
        CopyPass() = default;
        explicit CopyPass(SDL_GPUCopyPass* pass) : m_pass(pass) {}
        ~CopyPass() { end(); }
        CopyPass(const CopyPass&) = delete;
        CopyPass& operator=(const CopyPass&) = delete;
        CopyPass(CopyPass&& other) noexcept;
        CopyPass& operator=(CopyPass&& other) noexcept;

        void end();
        SDL_GPUCopyPass* get() const { return m_pass; }
        explicit operator bool() const { return m_pass != nullptr; }

    private:
        SDL_GPUCopyPass* m_pass = nullptr;
    };

    // Command buffer de SDL GPU. Se envía con submit(); si se destruye sin enviar, se cancela
    // (nada de lo grabado se ejecuta). Solo se puede mover.
    class CommandBuffer {
    public:
        CommandBuffer() = default;
        CommandBuffer(SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd) : m_device(device), m_cmd(cmd) {}
        ~CommandBuffer();
        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;
        CommandBuffer(CommandBuffer&& other) noexcept;
        CommandBuffer& operator=(CommandBuffer&& other) noexcept;

        RenderPass beginRenderPass(const ColorTarget& color, const DepthTarget* depth = nullptr);
        CopyPass beginCopyPass();

        // Envía el trabajo a la GPU (no espera). false si falló (ver el log)
        bool submit();
        // Envía y bloquea hasta que la GPU termine (descargas, tests)
        bool submitAndWait();
        // Descarta lo grabado
        void cancel();

        SDL_GPUCommandBuffer* get() const { return m_cmd; }
        explicit operator bool() const { return m_cmd != nullptr; }

    private:
        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUCommandBuffer* m_cmd = nullptr;
    };
}
