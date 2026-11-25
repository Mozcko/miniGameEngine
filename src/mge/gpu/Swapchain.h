#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>

namespace mge {

    // Present mode selection (pure functions: tested without a GPU).
    // vsync: VSYNC (always available, no tearing). No vsync: MAILBOX (no tearing, low latency) if
    // supported, otherwise IMMEDIATE (may tear), and if neither is supported, VSYNC.
    using PresentModeSupport = std::function<bool(SDL_GPUPresentMode)>;
    SDL_GPUPresentMode choosePresentMode(bool vsync, const PresentModeSupport& supported);

    // Depth format: the most precise available (D32_FLOAT, D24_UNORM, D16_UNORM).
    // 24 or 32 bits avoid the distant z-fighting caused by the default 16-bit depth (KI#22).
    using TextureFormatSupport = std::function<bool(SDL_GPUTextureFormat)>;
    SDL_GPUTextureFormat chooseDepthFormat(const TextureFormatSupport& supported);

    const char* presentModeName(SDL_GPUPresentMode mode);
}
