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

    // --- CommandBuffer ---
    CommandBuffer::~CommandBuffer() {
        cancel();
    }

    CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
        : m_device(std::exchange(other.m_device, nullptr)), m_cmd(std::exchange(other.m_cmd, nullptr)) {}

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
        if (this != &other) {
            cancel();
            m_device = std::exchange(other.m_device, nullptr);
            m_cmd = std::exchange(other.m_cmd, nullptr);
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

    bool CommandBuffer::submit() {
        if (!m_cmd) return false;
        const bool ok = SDL_SubmitGPUCommandBuffer(std::exchange(m_cmd, nullptr));
        if (!ok) MGE_LOG_ERROR("Gpu", "No se pudo enviar el command buffer: ", SDL_GetError());
        return ok;
    }

    bool CommandBuffer::submitAndWait() {
        if (!m_cmd) return false;
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
        // Con una textura del swapchain adquirida SDL no deja cancelar: se envía vacío
        if (!SDL_CancelGPUCommandBuffer(m_cmd)) SDL_SubmitGPUCommandBuffer(m_cmd);
        m_cmd = nullptr;
    }
}
