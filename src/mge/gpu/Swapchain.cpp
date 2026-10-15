#include <mge/gpu/Swapchain.h>

namespace mge {

    SDL_GPUPresentMode choosePresentMode(bool vsync, const PresentModeSupport& supported) {
        if (vsync) return SDL_GPU_PRESENTMODE_VSYNC;
        if (supported(SDL_GPU_PRESENTMODE_MAILBOX)) return SDL_GPU_PRESENTMODE_MAILBOX;
        if (supported(SDL_GPU_PRESENTMODE_IMMEDIATE)) return SDL_GPU_PRESENTMODE_IMMEDIATE;
        return SDL_GPU_PRESENTMODE_VSYNC;
    }

    SDL_GPUTextureFormat chooseDepthFormat(const TextureFormatSupport& supported) {
        for (SDL_GPUTextureFormat format : {SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                            SDL_GPU_TEXTUREFORMAT_D16_UNORM}) {
            if (supported(format)) return format;
        }
        return SDL_GPU_TEXTUREFORMAT_D16_UNORM; // Obligatorio en todos los backends de SDL GPU
    }

    const char* presentModeName(SDL_GPUPresentMode mode) {
        switch (mode) {
        case SDL_GPU_PRESENTMODE_VSYNC: return "vsync";
        case SDL_GPU_PRESENTMODE_MAILBOX: return "mailbox";
        case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate";
        }
        return "?";
    }
}
