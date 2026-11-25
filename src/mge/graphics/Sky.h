#pragma once

#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/Pipeline.h>
#include <mge/graphics/DayNight.h>

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace mge {

    class MeshRenderer;

    // Procedural sky renderer (#35): draws a fullscreen triangle inside MeshRenderer's
    // render pass before opaque geometry. Reconstructs the view ray in world space
    // from inverseViewProjection and cameraPosition.
    // Note: Procedural clouds are planned as future work.
    class SkyRenderer {
    public:
        SkyRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

        bool valid() const { return static_cast<bool>(m_pipeline); }

        // Inside MeshRenderer's render pass, BEFORE opaque: fullscreen triangle,
        // no depth write, depth test ALWAYS (or at z = 1 with LESS_OR_EQUAL), color = sky by
        // view direction.
        void draw(MeshRenderer& renderer, const glm::mat4& inverseViewProjection,
                  const glm::vec3& cameraPosition, const SkySettings& sky);

    private:
        GpuGraphicsPipeline m_pipeline;
    };

} // namespace mge
