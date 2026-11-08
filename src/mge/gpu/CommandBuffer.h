#pragma once

#include <mge/gpu/Handles.h>

#include <SDL3/SDL_gpu.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <initializer_list>

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

    // Enlace de un storage buffer de lectura/escritura en un compute pass
    struct StorageBufferBinding {
        SDL_GPUBuffer* buffer = nullptr;
        bool cycle = false;

        StorageBufferBinding() = default;
        StorageBufferBinding(SDL_GPUBuffer* buf, bool cyc = false) : buffer(buf), cycle(cyc) {}
        StorageBufferBinding(const GpuBuffer& buf, bool cyc = false) : buffer(buf.get()), cycle(cyc) {}
    };

    // Enlace de una storage texture de lectura/escritura en un compute pass
    struct StorageTextureBinding {
        SDL_GPUTexture* texture = nullptr;
        std::uint32_t mipLevel = 0;
        std::uint32_t layer = 0;
        bool cycle = false;

        StorageTextureBinding() = default;
        StorageTextureBinding(SDL_GPUTexture* tex, bool cyc = false) : texture(tex), cycle(cyc) {}
        StorageTextureBinding(const GpuTexture& tex, bool cyc = false) : texture(tex.get()), cycle(cyc) {}
    };

    class CommandBuffer;

    // Compute pass abierto (shaders de cómputo): termina al destruirse (RAII)
    class ComputePass {
    public:
        ComputePass() = default;
        ComputePass(SDL_GPUComputePass* pass, CommandBuffer* cmd = nullptr);
        ~ComputePass() { end(); }
        ComputePass(const ComputePass&) = delete;
        ComputePass& operator=(const ComputePass&) = delete;
        ComputePass(ComputePass&& other) noexcept;
        ComputePass& operator=(ComputePass&& other) noexcept;

        void end();
        SDL_GPUComputePass* get() const { return m_pass; }
        explicit operator bool() const { return m_pass != nullptr; }

        void bindPipeline(SDL_GPUComputePipeline* pipeline);
        void bindPipeline(const GpuComputePipeline& pipeline);
        void bindStorageBuffers(std::uint32_t firstSlot, SDL_GPUBuffer* const* storageBuffers,
                                std::uint32_t numBindings);
        void bindStorageBuffers(std::uint32_t firstSlot, const GpuBuffer& buffer);
        void bindStorageBuffers(std::uint32_t firstSlot, std::initializer_list<SDL_GPUBuffer*> buffers);
        void pushUniform(std::uint32_t slotIndex, const void* data, std::uint32_t length);
        template <typename T> void pushUniform(std::uint32_t slotIndex, const T& data) {
            pushUniform(slotIndex, &data, static_cast<std::uint32_t>(sizeof(T)));
        }
        void dispatch(std::uint32_t groupcountX, std::uint32_t groupcountY = 1,
                      std::uint32_t groupcountZ = 1);

    private:
        friend class CommandBuffer;
        void setCommandBuffer(CommandBuffer* cmd) { m_cmd = cmd; }

        SDL_GPUComputePass* m_pass = nullptr;
        CommandBuffer* m_cmd = nullptr;
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

        // --- Compute ---
        ComputePass beginComputePass(const StorageTextureBinding* storageTextures = nullptr,
                                     std::uint32_t numStorageTextures = 0,
                                     const StorageBufferBinding* storageBuffers = nullptr,
                                     std::uint32_t numStorageBuffers = 0);
        ComputePass beginComputePass(const StorageBufferBinding& storageBuffer);
        ComputePass beginComputePass(const GpuBuffer& storageBuffer, bool cycle = false);
        ComputePass beginComputePass(std::initializer_list<StorageBufferBinding> storageBuffers);

        void bindComputePipeline(SDL_GPUComputePipeline* pipeline);
        void bindComputePipeline(const GpuComputePipeline& pipeline);
        void bindStorageBuffers(std::uint32_t firstSlot, SDL_GPUBuffer* const* storageBuffers,
                                std::uint32_t numBindings);
        void bindStorageBuffers(std::uint32_t firstSlot, const GpuBuffer& buffer);
        void bindStorageBuffers(std::uint32_t firstSlot, std::initializer_list<SDL_GPUBuffer*> buffers);
        void pushComputeUniform(std::uint32_t slotIndex, const void* data, std::uint32_t length);
        template <typename T> void pushComputeUniform(std::uint32_t slotIndex, const T& data) {
            pushComputeUniform(slotIndex, &data, static_cast<std::uint32_t>(sizeof(T)));
        }
        void dispatch(std::uint32_t groupcountX, std::uint32_t groupcountY = 1,
                      std::uint32_t groupcountZ = 1);
        void endComputePass();

        // Envía el trabajo a la GPU (no espera). false si falló (ver el log)
        bool submit();
        // Envía y bloquea hasta que la GPU termine (descargas, tests)
        bool submitAndWait();
        // Descarta lo grabado
        void cancel();

        SDL_GPUCommandBuffer* get() const { return m_cmd; }
        explicit operator bool() const { return m_cmd != nullptr; }

    private:
        friend class ComputePass;
        void setActiveComputePass(ComputePass* pass) { m_activeComputePass = pass; }
        void clearActiveComputePass(ComputePass* pass) {
            if (m_activeComputePass == pass) m_activeComputePass = nullptr;
            m_computePass = nullptr;
        }

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUCommandBuffer* m_cmd = nullptr;
        SDL_GPUComputePass* m_computePass = nullptr;
        ComputePass* m_activeComputePass = nullptr;
    };
}
