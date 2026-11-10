#pragma once

#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/Handles.h>
#include <mge/gpu/Pipeline.h>
#include <mge/gpu/Shader.h>
#include <mge/graphics/Lighting.h>

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace mge {

    class MeshRenderer;

    using ShaderBytecode = ShaderCode;

    // Parámetros por frame para el pase de SDF raymarching (#36)
    struct SdfFrame {
        glm::mat4 viewProjection{1.0f};
        glm::mat4 inverseViewProjection{1.0f};
        glm::vec3 cameraPosition{0.0f};
        float time = 0.0f;
        glm::vec4 params[4]{}; // Libres para el juego (posiciones, colores…)
    };

    // Renderizador de campos de distancia con signo (SDF raymarching pass, #36).
    // Dibuja un triángulo de pantalla completa dentro del render pass de MeshRenderer,
    // DESPUÉS de los objetos opacos y ANTES de los transparentes, con prueba y escritura de
    // profundidad (gl_FragDepth) para intersección correcta con geometría rasterizada.
    class SdfRenderer {
    public:
        // fragment = shader del juego (mge_add_shaders) que define map() y material() e incluye
        // mge/sdf_pass.glsl
        SdfRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat,
                    const ShaderBytecode& fragment);

        bool valid() const { return static_cast<bool>(m_pipeline); }

        // Dentro del render pass de MeshRenderer, DESPUÉS de lo opaco y antes de lo transparente
        void draw(MeshRenderer& renderer, const SdfFrame& frame, const Lighting& lighting);

        float maxDistance = 200.0f;
        int maxSteps = 128;

    private:
        GpuGraphicsPipeline m_pipeline;
    };

} // namespace mge
