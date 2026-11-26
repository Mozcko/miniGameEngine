// mge_graphics: Texture2D (orientation, regions) and MeshRenderer, including the sandbox scene.
#include "../gpu/GpuTestContext.h"
#include "RenderTest.h"

#include <game/SandboxBlocks.h>
#include <game/SandboxMaterials.h>
#include <game/SandboxTerrain.h>
#include <mge/gltf/GltfLoader.h>
#include <mge/graphics/DebugDraw.h>
#include <mge/graphics/GpuModel.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/graphics/SdfRenderer.h>
#include <mge/graphics/Texture2D.h>
#include <mge/procgen/Image.h>
#include <mge/procgen/Primitives.h>
#include <mge/procgen/Sdf.h>
#include <mge/voxel/ChunkMesher.h>

#include <shaders/sdf_clipped_sphere_frag.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

using namespace mge;
using mge_test::GpuTestContext;

namespace {
    // Fullscreen quad (NDC), counter-clockwise, UV with (0, 0) at bottom-left
    MeshData fullscreenQuad() {
        MeshData quad;
        const glm::vec3 n(0.0f, 0.0f, 1.0f);
        const auto a = quad.addVertex({-1, -1, 0}, n, glm::vec4(1.0f), {0, 0});
        const auto b = quad.addVertex({1, -1, 0}, n, glm::vec4(1.0f), {1, 0});
        const auto c = quad.addVertex({1, 1, 0}, n, glm::vec4(1.0f), {1, 1});
        const auto d = quad.addVertex({-1, 1, 0}, n, glm::vec4(1.0f), {0, 1});
        quad.addQuad(a, b, c, d);
        return quad;
    }

    bool isColor(const std::uint8_t* pixel, int r, int g, int b) {
        return pixel[0] == r && pixel[1] == g && pixel[2] == b;
    }

    // Renders the texture fullscreen with MeshRenderer and returns the image
    mge_test::Image renderTexture(GpuDevice& device, MeshRenderer& renderer, const GpuMesh& quad,
                                  const Texture2D& texture) {
        constexpr std::uint32_t kSize = 64;
        GpuTexture target = device.createRenderTarget(kSize, kSize);
        CommandBuffer cmd = device.acquireCommandBuffer();
        if (renderer.begin(cmd, target.get(), kSize, kSize, {0, 0, 0, 1})) {
            renderer.setCamera(glm::mat4(1.0f));
            renderer.draw(quad, glm::mat4(1.0f), &texture);
            renderer.end();
        }
        cmd.submitAndWait();
        return {kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    }
}

TEST_CASE("Texture2D: image row 0 appears at top of screen (UV with v-up)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    const GpuMesh quad = device.uploadMesh(fullscreenQuad());

    // 2x2 image, row 0 at top: red, green / blue, white
    const std::uint8_t pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Texture2D texture = Texture2D::fromPixels(device, 2, 2, pixels, true);
    REQUIRE(texture);
    CHECK(texture.width() == 2);
    CHECK(texture.originalPixels().size() == 16);

    auto image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(8, 8), 255, 0, 0));       // Top-left
    CHECK(isColor(image.pixel(56, 8), 0, 255, 0));      // Top-right
    CHECK(isColor(image.pixel(8, 56), 0, 0, 255));      // Bottom-left
    CHECK(isColor(image.pixel(56, 56), 255, 255, 255)); // Bottom-right
    CHECK(renderer.drawCalls() == 1);
    CHECK(renderer.triangles() == 2);

    // updateRegion uses image coordinates (row 0 at top)
    const std::uint8_t yellow[] = {255, 255, 0, 255};
    REQUIRE(texture.updateRegion(device, 1, 1, 1, 1, yellow));
    image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(56, 56), 255, 255, 0));
    CHECK(isColor(image.pixel(8, 8), 255, 0, 0)); // Rest unchanged

    // copyFromOriginal copies from the original image (bottom-right white, not yellow)
    REQUIRE(texture.copyFromOriginal(device, 1, 1, 1, 1, 0, 0));
    image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(8, 8), 255, 255, 255));
    CHECK_FALSE(texture.updateRegion(device, 2, 0, 1, 1, yellow)); // Out of bounds
}

TEST_CASE("Texture2D: non-existent file returns an invalid texture") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    CHECK_FALSE(Texture2D::fromFile(gpu.device(), "no/existe.png"));
}

namespace {
    // Fixed sandbox scene (default seed) with the given atlas
    void sandboxScene(const char* golden, bool procedural) {
        GpuTestContext gpu;
        if (!gpu.available()) return;
        GpuDevice& device = gpu.device();

        BlockRegistry registry;
        const sandbox::SandboxBlocks blocks = sandbox::registerSandboxBlocks(registry);
        auto terrain = std::make_unique<sandbox::SandboxTerrain>(blocks);
        const int ground = std::max(terrain->surfaceHeight(0, 0), sandbox::SandboxTerrain::kWaterLevel);
        VoxelWorld world(registry, std::move(terrain));
        for (int x = -2; x <= 2; x++)
            for (int z = -2; z <= 2; z++)
                for (int y = 0; y < sandbox::SandboxTerrain::kWorldChunksY; y++) world.loadChunk({x, y, z});

        Texture2D atlas;
        if (procedural) {
            const Image image = sandbox::buildProceduralAtlas();
            writePng(image,
                     std::filesystem::path(MGE_RENDER_OUTPUT_DIR) / "procedural_atlas.png"); // For inspection
            atlas = Texture2D::fromPixels(device, image.width(), image.height(), image.data());
        } else {
            atlas = Texture2D::fromFile(device, MGE_TEST_ASSET_DIR "/textures/Minecraft-like_3d_tileset.png");
        }
        REQUIRE(atlas);
        MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        REQUIRE(renderer.valid());

        ChunkMeshOptions options;
        options.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
        struct Chunk {
            GpuMesh opaque, cutout, transparent;
        };
        std::map<ChunkCoord, Chunk> meshes;
        for (int x = -1; x <= 1; x++)
            for (int z = -1; z <= 1; z++)
                for (int y = 0; y < sandbox::SandboxTerrain::kWorldChunksY; y++) {
                    ChunkMesh mesh = buildChunkMesh(world, {x, y, z}, options);
                    meshes[{x, y, z}] = {device.uploadMesh(mesh.opaque), device.uploadMesh(mesh.cutout),
                                         device.uploadMesh(mesh.transparent)};
                }

        const glm::vec3 eye(-14.0f, static_cast<float>(ground) + 9.0f, -14.0f);
        const glm::mat4 view =
            glm::lookAt(eye, glm::vec3(4.0f, static_cast<float>(ground) - 2.0f, 4.0f), {0, 1, 0});
        const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(60.0f), 1.0f, 0.1f, 200.0f);

        constexpr std::uint32_t kSize = 192;
        GpuTexture target = device.createRenderTarget(kSize, kSize);
        CommandBuffer cmd = device.acquireCommandBuffer();
        REQUIRE(renderer.begin(cmd, target.get(), kSize, kSize, {0.5f, 0.7f, 1.0f, 1.0f}));
        renderer.setCamera(projection * view);
        RenderQueue queue;
        for (const auto& [coord, chunk] : meshes) {
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkOrigin(coord)));
            const glm::vec3 center = glm::vec3(chunkOrigin(coord)) + glm::vec3(kChunkSize * 0.5f);
            queue.add({&chunk.opaque, model, &atlas, MeshPass::Opaque, center, {}});
            queue.add({&chunk.cutout, model, &atlas, MeshPass::Cutout, center, {}});
            queue.add({&chunk.transparent, model, &atlas, MeshPass::Transparent, center, {}});
        }
        queue.sort(eye, glm::normalize(glm::vec3(4.0f, static_cast<float>(ground) - 2.0f, 4.0f) - eye));
        renderer.submit(queue);
        renderer.end();
        REQUIRE(cmd.submitAndWait());
        CHECK(renderer.drawCalls() > 9);
        CHECK(renderer.stats().triangles[static_cast<std::size_t>(MeshPass::Cutout)] >
              0); // Leaves and flowers

        const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
        // Default tolerance (1%): Intel and lavapipe differ by ~0.3% (texel and triangle edges)
        const mge_test::GoldenResult result = mge_test::compareWithGolden(golden, image);
        if (!result.message.empty()) MESSAGE(result.message);
        CHECK(result.passed);
    }
}

TEST_CASE("Render: sandbox scene (SandboxTerrain + PNG atlas + MeshRenderer)") {
    sandboxScene("sandbox_scene", false);
}

TEST_CASE("Render: sandbox scene with procedural atlas (no image files, #34)") {
    sandboxScene("sandbox_scene_procedural", true);
}

TEST_CASE("RenderQueue: sorted by pass; opaque and cutout front-to-back, transparent back-to-front") {
    // Fake "non-empty" meshes: the queue only inspects indexCount
    GpuMesh mesh;
    mesh.indexCount = 3;
    RenderQueue queue;
    const glm::vec3 near(0, 0, -2), mid(0, 0, -10), far(0, 0, -30);
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Transparent, near, {}});
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Opaque, far, {}});
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Cutout, mid, {}});
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Transparent, far, {}});
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Opaque, near, {}});
    queue.add({&mesh, glm::mat4(1.0f), nullptr, MeshPass::Cutout, near, {}});
    GpuMesh empty;
    queue.add({&empty, glm::mat4(1.0f), nullptr, MeshPass::Opaque, near, {}}); // Empty: skipped
    CHECK(queue.size() == 6);

    queue.sort(glm::vec3(0.0f), glm::vec3(0, 0, -1));
    const auto& items = queue.items();
    auto is = [&](std::size_t i, MeshPass pass, const glm::vec3& center) {
        return items[i].pass == pass && items[i].center == center;
    };
    CHECK(is(0, MeshPass::Opaque, near));
    CHECK(is(1, MeshPass::Opaque, far));
    CHECK(is(2, MeshPass::Cutout, near));
    CHECK(is(3, MeshPass::Cutout, mid));
    CHECK(is(4, MeshPass::Transparent, far)); // Farthest first
    CHECK(is(5, MeshPass::Transparent, near));
}

TEST_CASE("Render: cutout leaves in front of transparent water and a flower on grass") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    BlockRegistry registry;
    const sandbox::SandboxBlocks b = sandbox::registerSandboxBlocks(registry);
    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    for (int x = 0; x < 8; x++)
        for (int z = 0; z < 8; z++) world.setBlock({x, 0, z}, b.grass);
    for (int x = 4; x < 8; x++)
        for (int z = 0; z < 4; z++) world.setBlock({x, 1, z}, b.water); // Pool in the back
    world.setBlock({5, 2, 5}, b.leaves);                                // Leaves between camera and water
    world.setBlock({5, 3, 5}, b.leaves);
    world.setBlock({2, 1, 5}, b.flower);
    world.setBlock({3, 1, 4}, b.rose);

    ChunkMeshOptions options;
    options.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, options);
    REQUIRE_FALSE(mesh.cutout.empty());
    REQUIRE_FALSE(mesh.transparent.empty());
    const GpuMesh opaque = device.uploadMesh(mesh.opaque);
    const GpuMesh cutout = device.uploadMesh(mesh.cutout);
    const GpuMesh transparent = device.uploadMesh(mesh.transparent);

    const Texture2D atlas =
        Texture2D::fromFile(device, MGE_TEST_ASSET_DIR "/textures/Minecraft-like_3d_tileset.png");
    REQUIRE(atlas);
    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    const glm::vec3 eye(4.0f, 5.5f, 11.0f), target(5.0f, 1.5f, 2.0f);
    const glm::mat4 viewProjection = glm::perspectiveRH_ZO(glm::radians(55.0f), 1.0f, 0.1f, 100.0f) *
                                     glm::lookAt(eye, target, glm::vec3(0, 1, 0));

    RenderQueue queue;
    // Intentionally added in reverse order: the queue sorts them
    queue.add({&transparent, glm::mat4(1.0f), &atlas, MeshPass::Transparent, glm::vec3(6, 1, 2), {}});
    queue.add({&cutout, glm::mat4(1.0f), &atlas, MeshPass::Cutout, glm::vec3(5, 2, 5), {}});
    queue.add({&opaque, glm::mat4(1.0f), &atlas, MeshPass::Opaque, glm::vec3(4, 0, 4), {}});
    queue.sort(eye, glm::normalize(target - eye));

    constexpr std::uint32_t kSize = 160;
    GpuTexture targetTexture = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, targetTexture.get(), kSize, kSize, {0.5f, 0.7f, 1.0f, 1.0f}));
    renderer.setCamera(viewProjection);
    renderer.submit(queue);
    renderer.end();
    REQUIRE(cmd.submitAndWait());
    CHECK(renderer.stats().drawCalls[static_cast<std::size_t>(MeshPass::Cutout)] == 1);

    const mge_test::Image image{kSize, kSize, device.downloadTexture(targetTexture, kSize, kSize)};
    const mge_test::GoldenResult result = mge_test::compareWithGolden("cutout_leaves_water", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("Render: untextured voxel scene with lighting, AO, and fog (#33)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    // Colored hills (no textures: lighting and AO alone define the shape)
    BlockRegistry registry;
    BlockDesc grass;
    grass.name = "grass";
    grass.color = {0.35f, 0.65f, 0.30f, 1.0f};
    BlockDesc rock;
    rock.name = "rock";
    rock.color = {0.55f, 0.55f, 0.58f, 1.0f};
    const BlockId g = registry.add(grass), r = registry.add(rock);
    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    for (int x = 0; x < kChunkSize; x++)
        for (int z = 0; z < kChunkSize; z++) {
            const int h = 2 + static_cast<int>(3.0f + 3.0f * std::sin(x * 0.5f) * std::cos(z * 0.4f));
            for (int y = 0; y <= h; y++) world.setBlock({x, y, z}, y == h ? g : r);
        }

    ChunkMeshOptions options;
    options.ambientOcclusion = true; // Greedy enabled: merges faces with uniform AO
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, options);
    const GpuMesh terrain = device.uploadMesh(mesh.opaque);

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    Lighting lighting;
    lighting.fogColor = {0.6f, 0.75f, 0.9f};
    lighting.fogStart = 12.0f;
    lighting.fogEnd = 34.0f;
    renderer.setLighting(lighting);
    const glm::vec3 eye(-6.0f, 14.0f, 22.0f);
    const glm::mat4 viewProjection = glm::perspectiveRH_ZO(glm::radians(55.0f), 1.0f, 0.1f, 100.0f) *
                                     glm::lookAt(eye, glm::vec3(8.0f, 3.0f, 6.0f), glm::vec3(0, 1, 0));

    constexpr std::uint32_t kSize = 160;
    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, target.get(), kSize, kSize, glm::vec4(lighting.fogColor, 1.0f)));
    renderer.setCamera(viewProjection, eye);
    renderer.draw(terrain, glm::mat4(1.0f));
    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    const mge_test::GoldenResult result = mge_test::compareWithGolden("lit_color_voxels", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("Lighting::unlit preserves texture * color unchanged") {
    const Lighting unlit = Lighting::unlit();
    CHECK(unlit.sunColor == glm::vec3(0.0f));
    CHECK(unlit.skyAmbient == glm::vec3(1.0f));
    CHECK(unlit.groundAmbient == glm::vec3(1.0f));
    CHECK_FALSE(unlit.fog);
}

TEST_CASE("Render: procedural detail on solid-color blocks (MeshMaterial, #34)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    BlockRegistry registry;
    BlockDesc stone;
    stone.name = "stone";
    stone.color = {0.55f, 0.55f, 0.58f, 1.0f};
    const BlockId s = registry.add(stone);
    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    for (int x = 0; x < 8; x++)
        for (int z = 0; z < 8; z++)
            for (int y = 0; y <= (x + z) / 4; y++) world.setBlock({x, y, z}, s); // Steps
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0});                     // Greedy: large flat faces
    const GpuMesh gpuMesh = device.uploadMesh(mesh.opaque);

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    renderer.setLighting(Lighting());
    const glm::vec3 eye(-4.0f, 9.0f, 13.0f);
    MeshMaterial material;
    material.detailScale = 2.0f;
    material.detailStrength = 0.25f;

    constexpr std::uint32_t kSize = 128;
    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, target.get(), kSize, kSize, {0.5f, 0.7f, 1.0f, 1.0f}));
    renderer.setCamera(glm::perspectiveRH_ZO(glm::radians(55.0f), 1.0f, 0.1f, 100.0f) *
                           glm::lookAt(eye, glm::vec3(4.0f, 1.0f, 4.0f), glm::vec3(0, 1, 0)),
                       eye);
    renderer.draw(gpuMesh, glm::mat4(1.0f), nullptr, MeshPass::Opaque, material);
    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    const mge_test::GoldenResult result = mge_test::compareWithGolden("color_detail_material", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("DebugLines: line count per shape and frustum corners") {
    DebugLines lines;
    lines.line({0, 0, 0}, {1, 0, 0}, {1, 1, 1, 1});
    CHECK(lines.lineCount() == 1);
    lines.aabb(Aabb({0, 0, 0}, {1, 2, 3}), {1, 0, 0, 1});
    CHECK(lines.lineCount() == 1 + 12);
    lines.aabb(Aabb(), {1, 0, 0, 1}); // Empty: nothing
    CHECK(lines.lineCount() == 13);
    lines.sphere({0, 0, 0}, 1.0f, {0, 1, 0, 1}, 16);
    CHECK(lines.lineCount() == 13 + 3 * 16);
    lines.axes({0, 0, 0}, 2.0f);
    CHECK(lines.lineCount() == 13 + 48 + 3);
    CHECK(lines.vertices().back().position == glm::vec3(0, 0, 2)); // Z axis
    lines.clear();

    // Frustum of a camera at the origin looking down -Z: far plane is at -far
    const glm::mat4 vp = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 1.0f, 10.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    lines.frustum(vp, {1, 1, 0, 1});
    CHECK(lines.lineCount() == 12);
    float minZ = 0.0f, maxZ = -100.0f;
    for (const GpuVertex& v : lines.vertices()) {
        minZ = std::min(minZ, v.position.z);
        maxZ = std::max(maxZ, v.position.z);
    }
    CHECK(minZ == doctest::Approx(-10.0f).epsilon(1e-3));
    CHECK(maxZ == doctest::Approx(-1.0f).epsilon(1e-3));
}

TEST_CASE("Render: debug lines (box, sphere, axes) with depth testing (#38)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    // Solid cube occluding part of the lines
    const GpuMesh cube = device.uploadMesh(primitives::box(glm::vec3(1.0f), {0.6f, 0.6f, 0.65f, 1.0f}));
    DebugDraw debug(device);
    debug.aabb(Aabb({-1.2f, -1.2f, -1.2f}, {1.2f, 1.2f, 1.2f}), {1.0f, 0.9f, 0.1f, 1.0f});
    debug.sphere({0, 0, 0}, 1.5f, {0.2f, 0.9f, 1.0f, 1.0f});
    debug.axes({0, 0, 0}, 2.0f);

    const glm::vec3 eye(3.0f, 2.5f, 4.0f);
    constexpr std::uint32_t kSize = 128;
    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, target.get(), kSize, kSize, {0.08f, 0.08f, 0.1f, 1.0f}));
    renderer.setCamera(glm::perspectiveRH_ZO(glm::radians(50.0f), 1.0f, 0.1f, 50.0f) *
                           glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0)),
                       eye);
    renderer.draw(cube, glm::mat4(1.0f));
    debug.flush(renderer);
    renderer.draw(cube, glm::translate(glm::mat4(1.0f), {0, -3, 0})); // Meshes after lines: still render fine
    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    // Default tolerance: Intel and lavapipe lines differ on ~0.5% of pixels
    const mge_test::GoldenResult result = mge_test::compareWithGolden("debug_lines", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("Render: SDF sphere intersecting and clipped by a rasterized box (#36)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());

    SdfRenderer sdfRenderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, device.depthFormat(),
                            shaders::kSdfClippedSphereFrag);
    REQUIRE(sdfRenderer.valid());

    // Grayish-blue rasterized box centered at (0, 0, 0) of size 2x2x2 (top at y = 1.0)
    const GpuMesh box =
        device.uploadMesh(primitives::box(glm::vec3(2.0f), glm::vec4(0.35f, 0.45f, 0.70f, 1.0f)));

    constexpr std::uint32_t kSize = 128;
    const glm::vec3 eye(2.2f, 3.2f, 3.8f);
    const glm::vec3 target(0.0f, 0.8f, 0.0f);
    const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(50.0f), 1.0f, 0.1f, 50.0f);
    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 viewProj = proj * view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    Lighting lighting;
    lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
    lighting.sunColor = glm::vec3(0.9f, 0.85f, 0.8f);
    lighting.skyAmbient = glm::vec3(0.5f, 0.55f, 0.65f);
    lighting.groundAmbient = glm::vec3(0.3f, 0.28f, 0.25f);
    lighting.fog = false;

    GpuTexture colorTarget = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, colorTarget.get(), kSize, kSize, {0.15f, 0.18f, 0.25f, 1.0f}));
    renderer.setCamera(viewProj, eye);
    renderer.setLighting(lighting);

    // 1. Draw opaque rasterized box
    renderer.draw(box, glm::mat4(1.0f));

    // 2. Draw SDF sphere (AFTER opaque geometry)
    SdfFrame frame;
    frame.viewProjection = viewProj;
    frame.inverseViewProjection = invViewProj;
    frame.cameraPosition = eye;
    frame.time = 0.0f;
    sdfRenderer.draw(renderer, frame, lighting);

    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(colorTarget, kSize, kSize)};

    // CPU verification: analytical raymarching and mge::sdf::sphere for several dome pixels
    for (auto [px, py] : {std::pair{64u, 42u}, std::pair{58u, 48u}, std::pair{70u, 45u}}) {
        // These pixels should show the sphere's orange color (high R, low B)
        const std::uint8_t* p = image.pixel(px, py);
        CHECK(p[0] > 140);
        CHECK(p[2] < 100);

        // CPU depth check using mge::sdf::sphere vs analytical intersection
        const float u = (float(px) + 0.5f) / float(kSize);
        const float v = 1.0f - (float(py) + 0.5f) / float(kSize);
        const glm::vec2 ndc(u * 2.0f - 1.0f, v * 2.0f - 1.0f);
        const glm::vec4 farTarget = invViewProj * glm::vec4(ndc, 1.0f, 1.0f);
        const glm::vec3 worldFar = glm::vec3(farTarget) / farTarget.w;
        const glm::vec3 ro = eye;
        const glm::vec3 rd = glm::normalize(worldFar - ro);

        // CPU raymarching with mge::sdf
        const glm::vec3 sphereCenter(0.0f, 1.0f, 0.0f);
        constexpr float kSphereRadius = 1.0f;
        auto sphereSdf = mge::sdf::opTranslate(mge::sdf::sphere(kSphereRadius), sphereCenter);

        float tCpu = 0.0f;
        bool hitCpu = false;
        for (int step = 0; step < 128 && tCpu < 20.0f; ++step) {
            float d = sphereSdf(ro + rd * tCpu);
            if (d < 0.001f) {
                hitCpu = true;
                break;
            }
            tCpu += d;
        }
        REQUIRE(hitCpu);

        // Analytical ray-sphere intersection
        const glm::vec3 oc = ro - sphereCenter;
        const float b = glm::dot(rd, oc);
        const float c = glm::dot(oc, oc) - kSphereRadius * kSphereRadius;
        const float disc = b * b - c;
        REQUIRE(disc >= 0.0f);
        const float tExact = -b - std::sqrt(disc);

        CHECK(tCpu == doctest::Approx(tExact).epsilon(0.002f));

        // Clip-space depth computed by the GPU (gl_FragDepth)
        const glm::vec3 hitPoint = ro + rd * tCpu;
        const glm::vec4 clipPos = viewProj * glm::vec4(hitPoint, 1.0f);
        const float depthCpu = clipPos.z / clipPos.w;
        const glm::vec3 hitExact = ro + rd * tExact;
        const glm::vec4 clipExact = viewProj * glm::vec4(hitExact, 1.0f);
        const float depthExact = clipExact.z / clipExact.w;

        CHECK(depthCpu == doctest::Approx(depthExact).epsilon(0.002f));
    }

    // Pixel where the sphere is submerged inside the box (e.g., x=64, y=75):
    // The box must have occluded the sphere (box color visible: low/medium R, higher B)
    const std::uint8_t* submergedPixel = image.pixel(64, 75);
    CHECK(submergedPixel[2] > submergedPixel[0]);

    const mge_test::GoldenResult result = mge_test::compareWithGolden("sdf_clipped_sphere", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("Render: transparent face sorting within a chunk (front and back views, #180)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    // Single chunk with two differently colored glass layers and water behind them (three translucent layers)
    BlockRegistry registry;
    BlockDesc stone;
    stone.name = "stone";
    stone.color = {0.42f, 0.42f, 0.45f, 1.0f};
    BlockDesc redGlass;
    redGlass.name = "red_glass";
    redGlass.opaque = false;
    redGlass.layer = RenderLayer::Translucent;
    redGlass.color = {0.95f, 0.12f, 0.12f, 0.55f};
    BlockDesc greenGlass;
    greenGlass.name = "green_glass";
    greenGlass.opaque = false;
    greenGlass.layer = RenderLayer::Translucent;
    greenGlass.color = {0.12f, 0.88f, 0.22f, 0.55f};
    BlockDesc water;
    water.name = "water";
    water.opaque = false;
    water.solid = false;
    water.layer = RenderLayer::Translucent;
    water.color = {0.10f, 0.35f, 0.95f, 0.65f};

    const BlockId sId = registry.add(stone);
    const BlockId rId = registry.add(redGlass);
    const BlockId gId = registry.add(greenGlass);
    const BlockId wId = registry.add(water);

    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    for (int x = 2; x <= 13; ++x)
        for (int z = 4; z <= 12; ++z) world.setBlock({x, 0, z}, sId);
    // Layer 1 (z = 6): red glass; Layer 2 (z = 8): green glass; Layer 3 (z = 10): blue water
    for (int y = 2; y <= 9; ++y) {
        for (int x = 3; x <= 10; ++x) world.setBlock({x, y, 6}, rId);
        for (int x = 4; x <= 11; ++x) world.setBlock({x, y, 8}, gId);
        for (int x = 5; x <= 12; ++x) world.setBlock({x, y, 10}, wId);
    }

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0});
    REQUIRE_FALSE(mesh.transparent.empty());
    REQUIRE(mesh.transparentFaceCentroids.size() * 6 == mesh.transparent.indices.size());

    const GpuMesh opaque = device.uploadMesh(mesh.opaque);
    GpuMesh transparent = device.uploadMesh(mesh.transparent);
    REQUIRE_FALSE(transparent.empty());

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    constexpr std::uint32_t kSize = 160;
    const glm::vec3 target(7.5f, 4.8f, 8.5f);

    auto renderFrom = [&](const glm::vec3& eye, const std::string& goldenName) {
        const std::vector<std::uint32_t> sorted =
            sortQuadsBackToFront(mesh.transparent.indices, mesh.transparentFaceCentroids, eye);
        REQUIRE(device.updateBuffer(transparent.indices, sorted.data(),
                                    static_cast<std::uint32_t>(sorted.size() * sizeof(std::uint32_t)), 0,
                                    true));

        const glm::mat4 vp = glm::perspectiveRH_ZO(glm::radians(50.0f), 1.0f, 0.1f, 100.0f) *
                             glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
        GpuTexture colorTarget = device.createRenderTarget(kSize, kSize);
        CommandBuffer cmd = device.acquireCommandBuffer();
        REQUIRE(renderer.begin(cmd, colorTarget.get(), kSize, kSize, {0.15f, 0.18f, 0.24f, 1.0f}));
        renderer.setCamera(vp, eye);
        renderer.draw(opaque, glm::mat4(1.0f), nullptr, MeshPass::Opaque);
        renderer.draw(transparent, glm::mat4(1.0f), nullptr, MeshPass::Transparent);
        renderer.end();
        REQUIRE(cmd.submitAndWait());

        const mge_test::Image image{kSize, kSize, device.downloadTexture(colorTarget, kSize, kSize)};
        const mge_test::GoldenResult result = mge_test::compareWithGolden(goldenName, image);
        if (!result.message.empty()) MESSAGE(result.message);
        CHECK(result.passed);
        return image;
    };

    // Front view (z < 6): red glass in front -> red dominates in the overlap area
    const mge_test::Image front = renderFrom(glm::vec3(7.5f, 7.5f, -5.0f), "transparent_sort_front");
    // Back view (z > 10): blue water in front -> blue dominates in the overlap area
    const mge_test::Image back = renderFrom(glm::vec3(7.5f, 7.5f, 22.0f), "transparent_sort_back");

    const std::uint8_t* frontPx = front.pixel(kSize / 2, kSize / 2);
    const std::uint8_t* backPx = back.pixel(kSize / 2, kSize / 2);
    CHECK(frontPx[0] > frontPx[2]); // Red > Blue in front view
    CHECK(backPx[2] > backPx[0]);   // Blue > Red in back view
    CHECK(frontPx[0] > backPx[0]);  // More red from the front than from the back
    CHECK(backPx[2] > frontPx[2]);  // More blue from the back than from the front
}

TEST_CASE("Render: GpuModel upload, embedded PNG texture, and ECS hierarchy golden (#200)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    // 1. Build a small 16x16 checkerboard PNG in memory via mge::Image + writePng
    mge::Image checker(16, 16);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const bool odd = (((x / 4) + (y / 4)) % 2) != 0;
            checker.set(x, y,
                        odd ? glm::vec4(0.95f, 0.45f, 0.15f, 1.0f) : glm::vec4(0.95f, 0.92f, 0.82f, 1.0f));
        }
    }
    const std::filesystem::path tmpPng = std::filesystem::temp_directory_path() / "mge_gltf_golden_tex.png";
    REQUIRE(writePng(checker, tmpPng));
    std::vector<std::uint8_t> pngBytes;
    {
        std::ifstream in(tmpPng, std::ios::binary | std::ios::ate);
        REQUIRE(in.good());
        const auto sz = in.tellg();
        REQUIRE(sz > 0);
        in.seekg(0, std::ios::beg);
        pngBytes.resize(static_cast<std::size_t>(sz));
        REQUIRE(static_cast<bool>(in.read(reinterpret_cast<char*>(pngBytes.data()), sz)));
    }
    std::filesystem::remove(tmpPng);

    // 2. Pack a cube (positions, normals, UVs, indices) + embedded PNG into a GLB binary chunk
    const MeshData cube = primitives::box(glm::vec3(1.2f));
    std::vector<std::uint8_t> bin;
    auto appendRaw = [&](const void* ptr, std::size_t bytes) -> std::pair<std::size_t, std::size_t> {
        while (bin.size() % 4 != 0) bin.push_back(0u);
        const std::size_t offset = bin.size();
        bin.resize(offset + bytes);
        std::memcpy(bin.data() + offset, ptr, bytes);
        return {offset, bytes};
    };
    const auto [posOff, posLen] = appendRaw(cube.positions.data(), cube.positions.size() * sizeof(glm::vec3));
    const auto [nrmOff, nrmLen] = appendRaw(cube.normals.data(), cube.normals.size() * sizeof(glm::vec3));
    const auto [uvOff, uvLen] = appendRaw(cube.uvs.data(), cube.uvs.size() * sizeof(glm::vec2));
    const auto [idxOff, idxLen] = appendRaw(cube.indices.data(), cube.indices.size() * sizeof(std::uint32_t));
    const auto [pngOff, pngLen] = appendRaw(pngBytes.data(), pngBytes.size());

    const std::string json =
        std::string(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)") + std::to_string(bin.size()) +
        R"(}],"bufferViews":[)" + R"({"buffer":0,"byteOffset":)" + std::to_string(posOff) +
        R"(,"byteLength":)" + std::to_string(posLen) + R"(},)" + R"({"buffer":0,"byteOffset":)" +
        std::to_string(nrmOff) + R"(,"byteLength":)" + std::to_string(nrmLen) + R"(},)" +
        R"({"buffer":0,"byteOffset":)" + std::to_string(uvOff) + R"(,"byteLength":)" + std::to_string(uvLen) +
        R"(},)" + R"({"buffer":0,"byteOffset":)" + std::to_string(idxOff) + R"(,"byteLength":)" +
        std::to_string(idxLen) + R"(},)" + R"({"buffer":0,"byteOffset":)" + std::to_string(pngOff) +
        R"(,"byteLength":)" + std::to_string(pngLen) + R"(}],)" + R"("accessors":[)" +
        R"({"bufferView":0,"componentType":5126,"count":)" + std::to_string(cube.positions.size()) +
        R"(,"type":"VEC3"},)" + R"({"bufferView":1,"componentType":5126,"count":)" +
        std::to_string(cube.normals.size()) + R"(,"type":"VEC3"},)" +
        R"({"bufferView":2,"componentType":5126,"count":)" + std::to_string(cube.uvs.size()) +
        R"(,"type":"VEC2"},)" + R"({"bufferView":3,"componentType":5125,"count":)" +
        std::to_string(cube.indices.size()) + R"(,"type":"SCALAR"}],)" +
        R"("images":[{"mimeType":"image/png","bufferView":4}],)" + R"("textures":[{"source":0}],)" +
        R"("materials":[)" +
        R"({"name":"Checker","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},)" +
        R"({"name":"Teal","pbrMetallicRoughness":{"baseColorFactor":[0.20,0.78,0.88,1.0]}}],)" +
        R"("meshes":[)" +
        R"({"name":"TexturedCube","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]},)" +
        R"({"name":"TintedCube","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":1}]}],)" +
        R"("nodes":[)" + R"({"name":"Parent","translation":[-0.85,0.0,0.0],"mesh":0,"children":[1]},)" +
        R"({"name":"Child","translation":[1.85,0.35,0.0],"scale":[0.75,0.75,0.75],"mesh":1}],)" +
        R"("scenes":[{"nodes":[0]}],"scene":0})";

    std::vector<std::uint8_t> jsonBytes(json.begin(), json.end());
    while (jsonBytes.size() % 4 != 0) jsonBytes.push_back(static_cast<std::uint8_t>(' '));
    std::vector<std::uint8_t> binPadded = bin;
    while (binPadded.size() % 4 != 0) binPadded.push_back(0u);

    const std::uint32_t totalLen = 12u + 8u + static_cast<std::uint32_t>(jsonBytes.size()) + 8u +
                                   static_cast<std::uint32_t>(binPadded.size());
    std::vector<std::uint8_t> glb;
    auto pushU32 = [&](std::uint32_t v) {
        glb.push_back(static_cast<std::uint8_t>(v & 0xFFu));
        glb.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
        glb.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
        glb.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    };
    pushU32(0x46546C67u);
    pushU32(2u);
    pushU32(totalLen);
    pushU32(static_cast<std::uint32_t>(jsonBytes.size()));
    pushU32(0x4E4F534Au);
    glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());
    pushU32(static_cast<std::uint32_t>(binPadded.size()));
    pushU32(0x004E4942u);
    glb.insert(glb.end(), binPadded.begin(), binPadded.end());

    // 3. Parse -> upload -> instantiate in ECS -> update transforms -> enqueue -> render
    const auto cpuModel = loadGltfFromMemory(glb.data(), glb.size());
    REQUIRE(cpuModel.has_value());

    GpuModel gpuModel = uploadModel(device, *cpuModel);
    REQUIRE(gpuModel.valid());
    REQUIRE(gpuModel.textures.size() == 1);
    CHECK(gpuModel.textures[0]);
    CHECK(gpuModel.textures[0].width() == 16);
    CHECK(gpuModel.textures[0].height() == 16);

    World world;
    const ModelInstance instance = instantiateModel(world, gpuModel);
    REQUIRE(instance.entities.size() == 2);
    REQUIRE(instance.rootEntities.size() == 1);
    CHECK(world.registry().has<Parent>(instance.entities[1]));
    CHECK(world.registry().get<Parent>(instance.entities[1]).entity == instance.entities[0]);

    TransformSystem::update(world.registry());

    RenderQueue queue;
    enqueueModelEntities(world, queue);
    CHECK(queue.size() == 2);

    const glm::vec3 eye(0.3f, 2.0f, 4.4f);
    const glm::vec3 target(0.1f, 0.1f, 0.0f);
    queue.sort(eye, glm::normalize(target - eye));

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    Lighting lighting;
    lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.5f));
    lighting.fog = false;
    renderer.setLighting(lighting);

    constexpr std::uint32_t kSize = 160;
    GpuTexture colorTarget = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(renderer.begin(cmd, colorTarget.get(), kSize, kSize, {0.14f, 0.17f, 0.22f, 1.0f}));
    renderer.setCamera(glm::perspectiveRH_ZO(glm::radians(48.0f), 1.0f, 0.1f, 50.0f) *
                           glm::lookAt(eye, target, {0, 1, 0}),
                       eye);
    renderer.submit(queue);
    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(colorTarget, kSize, kSize)};
    const mge_test::GoldenResult result = mge_test::compareWithGolden("gltf_model_hierarchy", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);

    // Explicit release before GpuDevice teardown
    gpuModel.clear();
    CHECK(gpuModel.empty());
}
