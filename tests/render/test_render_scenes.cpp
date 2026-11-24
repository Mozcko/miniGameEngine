// Renderer reference scenes (golden images). See RenderTest.h.
#include "../gpu/GpuTestContext.h"
#include "RenderTest.h"

#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/Pipeline.h>
#include <mge/procgen/MeshOps.h>
#include <mge/procgen/Primitives.h>

#include <shaders/lit_mesh_frag.h>
#include <shaders/lit_mesh_vert.h>
#include <shaders/vertex_colors_frag.h>
#include <shaders/vertex_colors_vert.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;
using mge_test::GpuTestContext;

namespace {
    constexpr std::uint32_t kSize = 128;

    void checkGolden(const std::string& name, const mge_test::Image& image) {
        REQUIRE(image.rgba.size() == std::size_t(image.width) * image.height * 4);
        const mge_test::GoldenResult result = mge_test::compareWithGolden(name, image);
        if (!result.message.empty()) MESSAGE(result.message);
        CHECK(result.passed);
    }
}

TEST_CASE("Render: clear image") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    checkGolden("clear",
                mge_test::renderOffscreen(gpu.device(), kSize, kSize, {0.2f, 0.4f, 0.6f, 1.0f}, false, {}));
}

TEST_CASE("Render: triangle with interpolated per-vertex colors") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    const GpuShader vs = device.createShader(shaders::kVertexColorsVert);
    const GpuShader fs = device.createShader(shaders::kVertexColorsFrag);
    GraphicsPipelineDesc desc;
    desc.vertexShader = &vs;
    desc.fragmentShader = &fs;
    desc.gpuVertexInput = false;
    const GpuGraphicsPipeline pipeline = device.createGraphicsPipeline(desc);
    REQUIRE(pipeline);

    // Back-face culling enabled: the triangle is counter-clockwise, so it should be visible
    const auto image = mge_test::renderOffscreen(device, kSize, kSize, {0.0f, 0.0f, 0.0f, 1.0f}, false,
                                                 [&](CommandBuffer&, RenderPass& pass) {
                                                     SDL_BindGPUGraphicsPipeline(pass.get(), pipeline.get());
                                                     SDL_DrawGPUPrimitives(pass.get(), 3, 1, 0, 0);
                                                 });
    // The top vertex (0, 0.8) is red and at the TOP of the image: NDC Y-up, row 0 at top
    CHECK(int(image.pixel(64, 20)[0]) > 150);
    CHECK(int(image.pixel(64, 120)[0]) == 0); // Below the triangle: background
    checkGolden("vertex_colors_triangle", image);
}

TEST_CASE("Render: lit cube with depth (MeshData -> GpuMesh)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ShaderResources vsResources;
    vsResources.uniformBuffers = 1;
    const GpuShader vs = device.createShader(shaders::kLitMeshVert, vsResources);
    const GpuShader fs = device.createShader(shaders::kLitMeshFrag);
    GraphicsPipelineDesc desc;
    desc.vertexShader = &vs;
    desc.fragmentShader = &fs;
    desc.depthFormat = device.depthFormat();
    const GpuGraphicsPipeline pipeline = device.createGraphicsPipeline(desc);
    REQUIRE(pipeline);

    // Two intersecting boxes: depth decides what is visible
    MeshData scene = primitives::box(glm::vec3(1.0f), {0.9f, 0.5f, 0.2f, 1.0f});
    MeshData second = primitives::box(glm::vec3(0.6f, 1.4f, 0.6f), {0.2f, 0.6f, 0.9f, 1.0f});
    transformMesh(second, glm::translate(glm::mat4(1.0f), {0.3f, 0.2f, 0.3f}));
    appendMesh(scene, second);
    const GpuMesh mesh = device.uploadMesh(scene);
    REQUIRE_FALSE(mesh.empty());

    struct Transforms {
        glm::mat4 viewProjection;
        glm::mat4 model;
    } transforms;
    // Projection with Z in [0, 1] (SDL GPU convention)
    const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(45.0f), 1.0f, 0.1f, 20.0f);
    const glm::mat4 view =
        glm::lookAt(glm::vec3(2.2f, 1.8f, 2.6f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    transforms.viewProjection = projection * view;
    transforms.model = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    const auto image = mge_test::renderOffscreen(
        device, kSize, kSize, {0.05f, 0.05f, 0.08f, 1.0f}, true, [&](CommandBuffer& cmd, RenderPass& pass) {
            SDL_PushGPUVertexUniformData(cmd.get(), 0, &transforms, sizeof(transforms));
            SDL_BindGPUGraphicsPipeline(pass.get(), pipeline.get());
            SDL_GPUBufferBinding vertices{mesh.vertices.get(), 0};
            SDL_GPUBufferBinding indices{mesh.indices.get(), 0};
            SDL_BindGPUVertexBuffers(pass.get(), 0, &vertices, 1);
            SDL_BindGPUIndexBuffer(pass.get(), &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_DrawGPUIndexedPrimitives(pass.get(), mesh.indexCount, 1, 0, 0, 0);
        });
    checkGolden("lit_boxes_depth", image);
}
