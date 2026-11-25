#pragma once

#include <mge/core/Module.h>
#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>

#include <cstdint>
#include <memory>

namespace mge {

    class PlatformModule;

    // What the game needs to draw the current frame
    struct GpuFrame {
        CommandBuffer* cmd = nullptr;        // Automatically submitted at the end of the frame
        SDL_GPUTexture* swapchain = nullptr; // The image presented to the window
        std::uint32_t width = 0;             // Actual swapchain size (changes on resize)
        std::uint32_t height = 0;
        // Display scale (SDL_GetWindowDisplayScale: DPI and user preference). For UI
        // (Renderer2D::begin); with the 8x8 font it is best rounded to an integer
        float displayScale = 1.0f;
        // Milliseconds this frame waited for the swapchain image (vsync or busy GPU): the rest of the
        // frame is CPU time. For the performance overlay (#43)
        float acquireMs = 0.0f;
    };

    // GPU module: creates the device (SDL3 GPU), claims the window from PlatformModule, and each frame
    // acquires a command buffer + swapchain image and submits them at frame end.
    // Add it AFTER PlatformModule: it initializes after (the window already exists) and shuts down before.
    class GpuModule : public Module {
    public:
        explicit GpuModule(GpuDeviceOptions options = {}) : m_options(options) {}

        const char* name() const override { return "Gpu"; }

        bool onInit(Application& app) override;
        void onFrameBegin(Application& app) override;
        void onFrameEnd(Application& app) override;
        void onShutdown(Application& app) override;

        // Valid between onInit and onShutdown
        GpuDevice& device() { return *m_device; }
        // nullptr if there is no image this frame (minimized window, or outside onUpdate/onRender)
        const GpuFrame* frame() const { return m_frame.swapchain ? &m_frame : nullptr; }
        SDL_GPUTextureFormat swapchainFormat() const;

        // Runtime vsync toggle (initial value comes from ApplicationConfig::vsync). Without vsync, uses
        // MAILBOX or IMMEDIATE if supported by the window (choosePresentMode); returns the resulting mode.
        SDL_GPUPresentMode setVsync(bool vsync);
        bool vsync() const { return m_vsync; }

    private:
        GpuDeviceOptions m_options;
        std::unique_ptr<GpuDevice> m_device;
        PlatformModule* m_platform = nullptr;
        CommandBuffer m_cmd;
        GpuFrame m_frame;
        bool m_vsync = true;
    };
}
