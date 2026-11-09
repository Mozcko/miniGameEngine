#pragma once

#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/Pipeline.h>
#include <mge/graphics/DayNight.h>

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace mge {

    class MeshRenderer;

    // Renderizador de cielo procedural (#35): dibuja un triángulo de pantalla completa dentro del
    // render pass de MeshRenderer antes de la geometría opaca. Reconstruye el rayo de vista en el
    // espacio del mundo a partir de inverseViewProjection y cameraPosition.
    // Nota: Las nubes procedurales quedan previstas como trabajo futuro.
    class SkyRenderer {
    public:
        SkyRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

        bool valid() const { return static_cast<bool>(m_pipeline); }

        // Dentro del render pass de MeshRenderer, ANTES de lo opaco: triángulo de pantalla completa,
        // sin depth write, depth test ALWAYS (o a z = 1 con LESS_OR_EQUAL), color = cielo según la
        // dirección de vista.
        void draw(MeshRenderer& renderer, const glm::mat4& inverseViewProjection,
                  const glm::vec3& cameraPosition, const SkySettings& sky);

    private:
        GpuGraphicsPipeline m_pipeline;
    };

} // namespace mge
