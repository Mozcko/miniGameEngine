#include "GpuTestContext.h"

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/procgen/Primitives.h>

#include <cstdint>
#include <type_traits>

using namespace mge;
using mge_test::GpuTestContext;

// Los handles son de dueño único: copiarlos está prohibido en tiempo de compilación
static_assert(!std::is_copy_constructible_v<GpuBuffer>);
static_assert(!std::is_copy_assignable_v<GpuTexture>);
static_assert(std::is_nothrow_move_constructible_v<GpuGraphicsPipeline>);
static_assert(!std::is_copy_constructible_v<CommandBuffer>);
static_assert(!std::is_copy_constructible_v<RenderPass>);

TEST_CASE("GpuVertex: MeshData entrelazado con todos los atributos (también el color)") {
    MeshData mesh = primitives::box(glm::vec3(1.0f), {0.2f, 0.4f, 0.6f, 1.0f});
    const std::vector<GpuVertex> vertices = interleave(mesh);
    REQUIRE(vertices.size() == mesh.vertexCount());
    for (std::size_t i = 0; i < vertices.size(); i++) {
        CHECK(vertices[i].position == mesh.positions[i]);
        CHECK(vertices[i].normal == mesh.normals[i]);
        CHECK(vertices[i].color == mesh.colors[i]);
        CHECK(vertices[i].uv == mesh.uvs[i]);
    }
    const auto attributes = gpuVertexAttributes();
    CHECK(gpuVertexBufferDescription().pitch == sizeof(GpuVertex));
    for (std::uint32_t i = 0; i < attributes.size(); i++) CHECK(attributes[i].location == i);
    CHECK(attributes[2].offset == 24); // color después de posición y normal
}

TEST_CASE("GPU: limpiar una textura y leerla de vuelta") {
    GpuTestContext gpu;
    if (!gpu.available()) return;

    GpuTexture target = gpu.device().createRenderTarget(16, 8);
    REQUIRE(target);
    CommandBuffer cmd = gpu.device().acquireCommandBuffer();
    REQUIRE(cmd);
    {
        ColorTarget color;
        color.texture = target.get();
        color.clearColor = {1.0f, 0.5f, 0.0f, 1.0f};
        RenderPass pass = cmd.beginRenderPass(color);
        CHECK(pass);
    }
    REQUIRE(cmd.submitAndWait());

    const std::vector<std::uint8_t> pixels = gpu.device().downloadTexture(target, 16, 8);
    REQUIRE(pixels.size() == 16 * 8 * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        CHECK(pixels[i] == 255);
        CHECK(int(pixels[i + 1]) == doctest::Approx(128).epsilon(0.01)); // 0.5 → 127 o 128
        CHECK(pixels[i + 2] == 0);
        CHECK(pixels[i + 3] == 255);
    }
    CHECK(GpuTestContext::validationMessages().empty());
}

TEST_CASE("GPU: subir una malla y liberar todo sin errores de validación") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    {
        GpuMesh mesh = gpu.device().uploadMesh(primitives::icosphere(1.0f, 2));
        CHECK_FALSE(mesh.empty());
        CHECK(mesh.vertices);
        CHECK(mesh.indices);
        CHECK(mesh.indexCount % 3 == 0);

        // Mover un handle transfiere el dueño: el original queda vacío y no se libera dos veces
        GpuBuffer moved = std::move(mesh.vertices);
        CHECK(moved);
        CHECK_FALSE(mesh.vertices);

        CHECK(gpu.device().uploadMesh(MeshData{}).empty()); // Malla vacía: nada que subir
        gpu.device().waitIdle();
    }
    CHECK(GpuTestContext::validationMessages().empty());
}

TEST_CASE("GPU: un command buffer sin enviar se cancela al destruirse") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    {
        CommandBuffer cmd = gpu.device().acquireCommandBuffer();
        CHECK(cmd);
        CommandBuffer other = std::move(cmd);
        CHECK_FALSE(cmd);
        CHECK(other);
    } // other se cancela aquí
    gpu.device().waitIdle();
    CHECK(GpuTestContext::validationMessages().empty());
}
