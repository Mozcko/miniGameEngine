#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>

namespace mge {

    // Elección del modo de presentación (funciones puras: se testean sin GPU).
    // vsync: VSYNC (siempre disponible, sin tearing). Sin vsync: MAILBOX (sin tearing, baja latencia) si
    // está, si no IMMEDIATE (puede haber tearing), y si ninguno está, VSYNC.
    using PresentModeSupport = std::function<bool(SDL_GPUPresentMode)>;
    SDL_GPUPresentMode choosePresentMode(bool vsync, const PresentModeSupport& supported);

    // Formato de profundidad: el más preciso disponible (D32_FLOAT, D24_UNORM, D16_UNORM).
    // 24 o 32 bits evitan el z-fighting a distancia que daba el depth de 16 bits por defecto (KI#22).
    using TextureFormatSupport = std::function<bool(SDL_GPUTextureFormat)>;
    SDL_GPUTextureFormat chooseDepthFormat(const TextureFormatSupport& supported);

    const char* presentModeName(SDL_GPUPresentMode mode);
}
