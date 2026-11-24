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

    // Per-frame parameters for the SDF raymarching pass (#36)
    struct SdfFrame {
        glm::mat4 viewProjection{1.0f};
        glm::mat4 inverseViewProjection{1.0f};
        glm::vec3 cameraPosition{0.0f};
        float time = 0.0f;
        glm::vec4 params[4]{}; // Free for game use (positions, colors...)
    };

    // Signed distance field renderer (SDF raymarching pass, #36).
    // Draws a fullscreen triangle inside MeshRenderer's render pass,
    // AFTER opaque objects and BEFORE transparent ones, with depth test and depth write
    // (gl_FragDepth) for proper intersection with rasterized geometry.
    class SdfRenderer {
    public:
        // fragment = game shader (mge_add_shaders) that defines map() and material() and includes
        // mge/sdf_pass.glsl
        SdfRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat,
                    const ShaderBytecode& fragment);

        bool valid() const { return static_cast<bool>(m_pipeline); }

        // Inside MeshRenderer's render pass, AFTER opaque and before transparent
        void draw(MeshRenderer& renderer, const SdfFrame& frame, const Lighting& lighting);

        float maxDistance = 200.0f;
        int maxSteps = 128;

    private:
        GpuGraphicsPipeline m_pipeline;
    };

} // namespace mge
