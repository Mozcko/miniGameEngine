#include "GpuTestContext.h"

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/procgen/Primitives.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

using namespace mge;
using mge_test::GpuTestContext;

// Handles are single-owner: copying is forbidden at compile time
static_assert(!std::is_copy_constructible_v<GpuBuffer>);
static_assert(!std::is_copy_assignable_v<GpuTexture>);
static_assert(std::is_nothrow_move_constructible_v<GpuGraphicsPipeline>);
static_assert(!std::is_copy_constructible_v<CommandBuffer>);
static_assert(!std::is_copy_constructible_v<RenderPass>);

TEST_CASE("GpuVertex: interleaved MeshData with all attributes (including color)") {
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
    CHECK(attributes[2].offset == 24); // color after position and normal
}

TEST_CASE("GPU: clear a texture and read it back") {
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
        CHECK(int(pixels[i + 1]) == doctest::Approx(128).epsilon(0.01)); // 0.5 -> 127 or 128
        CHECK(pixels[i + 2] == 0);
        CHECK(pixels[i + 3] == 255);
    }
}

TEST_CASE("GPU: upload a mesh and release everything without validation errors") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    {
        GpuMesh mesh = gpu.device().uploadMesh(primitives::icosphere(1.0f, 2));
        CHECK_FALSE(mesh.empty());
        CHECK(mesh.vertices);
        CHECK(mesh.indices);
        CHECK(mesh.indexCount % 3 == 0);

        // Moving a handle transfers ownership: the original becomes empty and is not freed twice
        GpuBuffer moved = std::move(mesh.vertices);
        CHECK(moved);
        CHECK_FALSE(mesh.vertices);

        CHECK(gpu.device().uploadMesh(MeshData{}).empty()); // Empty mesh: nothing to upload
        gpu.device().waitIdle();
    }
}

TEST_CASE("GPU: an unsubmitted command buffer is cancelled on destruction") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    {
        CommandBuffer cmd = gpu.device().acquireCommandBuffer();
        CHECK(cmd);
        CommandBuffer other = std::move(cmd);
        CHECK_FALSE(cmd);
        CHECK(other);
    } // other is cancelled here
    gpu.device().waitIdle();
}

TEST_CASE("GPU: GpuResourceTracker releases resources before destroying device and leaves handle inert") {
    GpuTestContext gpu;
    if (!gpu.available()) return;

    struct TrackedMesh {
        GpuMesh mesh;
        bool alive = true;
    };

    auto tracked = std::make_shared<TrackedMesh>();
    tracked->mesh = gpu.device().uploadMesh(primitives::box(glm::vec3(1.0f)));
    CHECK_FALSE(tracked->mesh.empty());

    gpu.device().tracker().registerResource("GpuMesh", [tracked]() {
        tracked->mesh = GpuMesh{};
        tracked->alive = false;
    });

    CHECK(gpu.device().tracker().trackedCount() == 1);
    CHECK(tracked->alive);

    // Simulate releaseAll() from onShutdown before destroying the device
    gpu.device().tracker().releaseAll();
    CHECK(gpu.device().tracker().trackedCount() == 0);
    CHECK_FALSE(tracked->alive);
    CHECK(tracked->mesh.empty());
}

TEST_CASE("GPU: updateBuffer updates an offset range and the full buffer in-place (#180)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    const std::uint32_t initial[6] = {1, 2, 3, 4, 5, 6};
    const GpuBuffer buf = device.createBuffer(SDL_GPU_BUFFERUSAGE_INDEX, sizeof(initial));
    REQUIRE(buf);
    REQUIRE(device.uploadToBuffer(buf, initial, sizeof(initial)));

    // Partial update with offset = 2 * sizeof(uint32_t)
    const std::uint32_t patch[2] = {30, 40};
    REQUIRE(device.updateBuffer(buf, patch, sizeof(patch), 2 * sizeof(std::uint32_t)));

    std::vector<std::uint8_t> raw = device.downloadBuffer(buf, sizeof(initial));
    REQUIRE(raw.size() == sizeof(initial));
    std::uint32_t readback[6]{};
    std::memcpy(readback, raw.data(), sizeof(readback));
    CHECK(readback[0] == 1);
    CHECK(readback[1] == 2);
    CHECK(readback[2] == 30);
    CHECK(readback[3] == 40);
    CHECK(readback[4] == 5);
    CHECK(readback[5] == 6);

    // Full update from offset 0 with cycle = true
    const std::uint32_t replaced[6] = {60, 50, 40, 30, 20, 10};
    REQUIRE(device.updateBuffer(buf, replaced, sizeof(replaced), 0, true));
    raw = device.downloadBuffer(buf, sizeof(replaced));
    REQUIRE(raw.size() == sizeof(replaced));
    std::memcpy(readback, raw.data(), sizeof(readback));
    for (int i = 0; i < 6; ++i) CHECK(readback[i] == replaced[i]);
}
