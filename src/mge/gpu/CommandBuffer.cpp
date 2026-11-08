#include <mge/gpu/CommandBuffer.h>

#include <mge/core/Log.h>

#include <utility>

namespace mge {

    // --- RenderPass ---
    RenderPass::RenderPass(RenderPass&& other) noexcept : m_pass(std::exchange(other.m_pass, nullptr)) {}

    RenderPass& RenderPass::operator=(RenderPass&& other) noexcept {
        if (this != &other) {
            end();
            m_pass = std::exchange(other.m_pass, nullptr);
        }
        return *this;
    }

    void RenderPass::end() {
        if (m_pass) SDL_EndGPURenderPass(m_pass);
        m_pass = nullptr;
    }

    // --- CopyPass ---
    CopyPass::CopyPass(CopyPass&& other) noexcept : m_pass(std::exchange(other.m_pass, nullptr)) {}

    CopyPass& CopyPass::operator=(CopyPass&& other) noexcept {
        if (this != &other) {
            end();
            m_pass = std::exchange(other.m_pass, nullptr);
        }
        return *this;
    }

    void CopyPass::end() {
        if (m_pass) SDL_EndGPUCopyPass(m_pass);
        m_pass = nullptr;
    }

    // --- ComputePass ---
    ComputePass::ComputePass(SDL_GPUComputePass* pass, CommandBuffer* cmd) : m_pass(pass), m_cmd(cmd) {
        if (m_cmd) {
            m_cmd->setActiveComputePass(this);
        }
    }

    ComputePass::ComputePass(ComputePass&& other) noexcept
        : m_pass(std::exchange(other.m_pass, nullptr)), m_cmd(std::exchange(other.m_cmd, nullptr)) {
        if (m_cmd) {
            m_cmd->setActiveComputePass(this);
        }
    }

    ComputePass& ComputePass::operator=(ComputePass&& other) noexcept {
        if (this != &other) {
            end();
            m_pass = std::exchange(other.m_pass, nullptr);
            m_cmd = std::exchange(other.m_cmd, nullptr);
            if (m_cmd) {
                m_cmd->setActiveComputePass(this);
            }
        }
        return *this;
    }

    void ComputePass::end() {
        if (m_cmd) {
            m_cmd->clearActiveComputePass(this);
            m_cmd = nullptr;
        }
        if (m_pass) {
            SDL_EndGPUComputePass(m_pass);
            m_pass = nullptr;
        }
    }

    void ComputePass::bindPipeline(SDL_GPUComputePipeline* pipeline) {
        if (m_pass) SDL_BindGPUComputePipeline(m_pass, pipeline);
    }

    void ComputePass::bindPipeline(const GpuComputePipeline& pipeline) {
        bindPipeline(pipeline.get());
    }

    void ComputePass::bindStorageBuffers(std::uint32_t firstSlot, SDL_GPUBuffer* const* storageBuffers,
                                         std::uint32_t numBindings) {
        if (m_pass) SDL_BindGPUComputeStorageBuffers(m_pass, firstSlot, storageBuffers, numBindings);
    }

    void ComputePass::bindStorageBuffers(std::uint32_t firstSlot, const GpuBuffer& buffer) {
        SDL_GPUBuffer* ptr = buffer.get();
        bindStorageBuffers(firstSlot, &ptr, 1);
    }

    void ComputePass::bindStorageBuffers(std::uint32_t firstSlot,
                                         std::initializer_list<SDL_GPUBuffer*> buffers) {
        bindStorageBuffers(firstSlot, buffers.begin(), static_cast<std::uint32_t>(buffers.size()));
    }

    void ComputePass::pushUniform(std::uint32_t slotIndex, const void* data, std::uint32_t length) {
        if (m_cmd) m_cmd->pushComputeUniform(slotIndex, data, length);
    }

    void ComputePass::dispatch(std::uint32_t groupcountX, std::uint32_t groupcountY,
                               std::uint32_t groupcountZ) {
        if (m_pass) SDL_DispatchGPUCompute(m_pass, groupcountX, groupcountY, groupcountZ);
    }

    // --- CommandBuffer ---
    CommandBuffer::~CommandBuffer() {
        cancel();
    }

    CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
        : m_device(std::exchange(other.m_device, nullptr)), m_cmd(std::exchange(other.m_cmd, nullptr)),
          m_computePass(std::exchange(other.m_computePass, nullptr)),
          m_activeComputePass(std::exchange(other.m_activeComputePass, nullptr)) {
        if (m_activeComputePass) {
            m_activeComputePass->setCommandBuffer(this);
        }
    }

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
        if (this != &other) {
            cancel();
            m_device = std::exchange(other.m_device, nullptr);
            m_cmd = std::exchange(other.m_cmd, nullptr);
            m_computePass = std::exchange(other.m_computePass, nullptr);
            m_activeComputePass = std::exchange(other.m_activeComputePass, nullptr);
            if (m_activeComputePass) {
                m_activeComputePass->setCommandBuffer(this);
            }
        }
        return *this;
    }

    RenderPass CommandBuffer::beginRenderPass(const ColorTarget& color, const DepthTarget* depth) {
        SDL_GPUColorTargetInfo colorInfo{};
        colorInfo.texture = color.texture;
        colorInfo.clear_color = {color.clearColor.r, color.clearColor.g, color.clearColor.b,
                                 color.clearColor.a};
        colorInfo.load_op = color.clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        colorInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthInfo{};
        if (depth) {
            depthInfo.texture = depth->texture;
            depthInfo.clear_depth = depth->clearDepth;
            depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        }
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_cmd, &colorInfo, 1, depth ? &depthInfo : nullptr);
        if (!pass) MGE_LOG_ERROR("Gpu", "No se pudo abrir el render pass: ", SDL_GetError());
        return RenderPass(pass);
    }

    CopyPass CommandBuffer::beginCopyPass() {
        SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(m_cmd);
        if (!pass) MGE_LOG_ERROR("Gpu", "No se pudo abrir el copy pass: ", SDL_GetError());
        return CopyPass(pass);
    }

    ComputePass CommandBuffer::beginComputePass(const StorageTextureBinding* storageTextures,
                                                std::uint32_t numStorageTextures,
                                                const StorageBufferBinding* storageBuffers,
                                                std::uint32_t numStorageBuffers) {
        if (m_computePass || m_activeComputePass) endComputePass();

        std::vector<SDL_GPUStorageTextureReadWriteBinding> texHeap;
        SDL_GPUStorageTextureReadWriteBinding texStack[8];
        SDL_GPUStorageTextureReadWriteBinding* texPtr = nullptr;
        if (numStorageTextures > 0) {
            if (numStorageTextures <= 8) {
                texPtr = texStack;
            } else {
                texHeap.resize(numStorageTextures);
                texPtr = texHeap.data();
            }
            for (std::uint32_t i = 0; i < numStorageTextures; ++i) {
                texPtr[i].texture = storageTextures[i].texture;
                texPtr[i].mip_level = storageTextures[i].mipLevel;
                texPtr[i].layer = storageTextures[i].layer;
                texPtr[i].cycle = storageTextures[i].cycle;
                texPtr[i].padding1 = 0;
                texPtr[i].padding2 = 0;
                texPtr[i].padding3 = 0;
            }
        }

        std::vector<SDL_GPUStorageBufferReadWriteBinding> bufHeap;
        SDL_GPUStorageBufferReadWriteBinding bufStack[8];
        SDL_GPUStorageBufferReadWriteBinding* bufPtr = nullptr;
        if (numStorageBuffers > 0) {
            if (numStorageBuffers <= 8) {
                bufPtr = bufStack;
            } else {
                bufHeap.resize(numStorageBuffers);
                bufPtr = bufHeap.data();
            }
            for (std::uint32_t i = 0; i < numStorageBuffers; ++i) {
                bufPtr[i].buffer = storageBuffers[i].buffer;
                bufPtr[i].cycle = storageBuffers[i].cycle;
                bufPtr[i].padding1 = 0;
                bufPtr[i].padding2 = 0;
                bufPtr[i].padding3 = 0;
            }
        }

        SDL_GPUComputePass* pass =
            SDL_BeginGPUComputePass(m_cmd, texPtr, numStorageTextures, bufPtr, numStorageBuffers);
        if (!pass) MGE_LOG_ERROR("Gpu", "No se pudo abrir el compute pass: ", SDL_GetError());
        m_computePass = pass;
        return ComputePass(pass, this);
    }

    ComputePass CommandBuffer::beginComputePass(const StorageBufferBinding& storageBuffer) {
        return beginComputePass(nullptr, 0, &storageBuffer, 1);
    }

    ComputePass CommandBuffer::beginComputePass(const GpuBuffer& storageBuffer, bool cycle) {
        StorageBufferBinding b(storageBuffer, cycle);
        return beginComputePass(nullptr, 0, &b, 1);
    }

    ComputePass CommandBuffer::beginComputePass(std::initializer_list<StorageBufferBinding> storageBuffers) {
        return beginComputePass(nullptr, 0, storageBuffers.begin(),
                                static_cast<std::uint32_t>(storageBuffers.size()));
    }

    void CommandBuffer::bindComputePipeline(SDL_GPUComputePipeline* pipeline) {
        if (m_computePass) SDL_BindGPUComputePipeline(m_computePass, pipeline);
    }

    void CommandBuffer::bindComputePipeline(const GpuComputePipeline& pipeline) {
        bindComputePipeline(pipeline.get());
    }

    void CommandBuffer::bindStorageBuffers(std::uint32_t firstSlot, SDL_GPUBuffer* const* storageBuffers,
                                           std::uint32_t numBindings) {
        if (m_computePass)
            SDL_BindGPUComputeStorageBuffers(m_computePass, firstSlot, storageBuffers, numBindings);
    }

    void CommandBuffer::bindStorageBuffers(std::uint32_t firstSlot, const GpuBuffer& buffer) {
        SDL_GPUBuffer* ptr = buffer.get();
        bindStorageBuffers(firstSlot, &ptr, 1);
    }

    void CommandBuffer::bindStorageBuffers(std::uint32_t firstSlot,
                                           std::initializer_list<SDL_GPUBuffer*> buffers) {
        bindStorageBuffers(firstSlot, buffers.begin(), static_cast<std::uint32_t>(buffers.size()));
    }

    void CommandBuffer::pushComputeUniform(std::uint32_t slotIndex, const void* data, std::uint32_t length) {
        if (m_cmd) SDL_PushGPUComputeUniformData(m_cmd, slotIndex, data, length);
    }

    void CommandBuffer::dispatch(std::uint32_t groupcountX, std::uint32_t groupcountY,
                                 std::uint32_t groupcountZ) {
        if (m_computePass) SDL_DispatchGPUCompute(m_computePass, groupcountX, groupcountY, groupcountZ);
    }

    void CommandBuffer::endComputePass() {
        if (m_activeComputePass) {
            m_activeComputePass->end();
        } else if (m_computePass) {
            SDL_EndGPUComputePass(m_computePass);
            m_computePass = nullptr;
        }
    }

    bool CommandBuffer::submit() {
        if (!m_cmd) return false;
        endComputePass();
        const bool ok = SDL_SubmitGPUCommandBuffer(std::exchange(m_cmd, nullptr));
        if (!ok) MGE_LOG_ERROR("Gpu", "No se pudo enviar el command buffer: ", SDL_GetError());
        return ok;
    }

    bool CommandBuffer::submitAndWait() {
        if (!m_cmd) return false;
        endComputePass();
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(m_cmd, nullptr));
        if (!fence) {
            MGE_LOG_ERROR("Gpu", "No se pudo enviar el command buffer: ", SDL_GetError());
            return false;
        }
        const bool ok = SDL_WaitForGPUFences(m_device, true, &fence, 1);
        SDL_ReleaseGPUFence(m_device, fence);
        return ok;
    }

    void CommandBuffer::cancel() {
        if (!m_cmd) return;
        endComputePass();
        // Con una textura del swapchain adquirida SDL no deja cancelar: se envía vacío
        if (!SDL_CancelGPUCommandBuffer(m_cmd)) SDL_SubmitGPUCommandBuffer(m_cmd);
        m_cmd = nullptr;
    }
}
