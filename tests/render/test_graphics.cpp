// mge_graphics: Texture2D (orientación, regiones) y MeshRenderer, incluida la escena del sandbox.
#include "../gpu/GpuTestContext.h"
#include "RenderTest.h"

#include <game/SandboxBlocks.h>
#include <game/SandboxMaterials.h>
#include <game/SandboxTerrain.h>
#include <mge/graphics/DebugDraw.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/procgen/Primitives.h>
#include <mge/graphics/Texture2D.h>
#include <mge/voxel/ChunkMesher.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>

using namespace mge;
using mge_test::GpuTestContext;

namespace {
    // Quad que cubre toda la imagen (NDC), antihorario, UV con (0, 0) abajo a la izquierda
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

    // Dibuja la textura en pantalla completa con el MeshRenderer y devuelve la imagen
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

TEST_CASE("Texture2D: la fila 0 de la imagen queda arriba en pantalla (UV con v hacia arriba)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    const GpuMesh quad = device.uploadMesh(fullscreenQuad());

    // Imagen 2×2, fila 0 arriba: rojo, verde / azul, blanco
    const std::uint8_t pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Texture2D texture = Texture2D::fromPixels(device, 2, 2, pixels, true);
    REQUIRE(texture);
    CHECK(texture.width() == 2);
    CHECK(texture.originalPixels().size() == 16);

    auto image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(8, 8), 255, 0, 0));       // Arriba a la izquierda
    CHECK(isColor(image.pixel(56, 8), 0, 255, 0));      // Arriba a la derecha
    CHECK(isColor(image.pixel(8, 56), 0, 0, 255));      // Abajo a la izquierda
    CHECK(isColor(image.pixel(56, 56), 255, 255, 255)); // Abajo a la derecha
    CHECK(renderer.drawCalls() == 1);
    CHECK(renderer.triangles() == 2);

    // updateRegion usa coordenadas de la imagen (fila 0 arriba)
    const std::uint8_t yellow[] = {255, 255, 0, 255};
    REQUIRE(texture.updateRegion(device, 1, 1, 1, 1, yellow));
    image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(56, 56), 255, 255, 0));
    CHECK(isColor(image.pixel(8, 8), 255, 0, 0)); // El resto no cambia

    // copyFromOriginal copia de la imagen original (el blanco de abajo a la derecha, no el amarillo)
    REQUIRE(texture.copyFromOriginal(device, 1, 1, 1, 1, 0, 0));
    image = renderTexture(device, renderer, quad, texture);
    CHECK(isColor(image.pixel(8, 8), 255, 255, 255));
    CHECK_FALSE(texture.updateRegion(device, 2, 0, 1, 1, yellow)); // Fuera de la imagen
}

TEST_CASE("Texture2D: un archivo que no existe da una textura inválida") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    CHECK_FALSE(Texture2D::fromFile(gpu.device(), "no/existe.png"));
}

namespace {
    // Escena fija del sandbox (semilla por defecto) con el atlas dado
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
                     std::filesystem::path(MGE_RENDER_OUTPUT_DIR) / "procedural_atlas.png"); // Para revisarlo
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
        CHECK(renderer.stats().triangles[static_cast<std::size_t>(MeshPass::Cutout)] > 0); // Hojas y flores

        const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
        // Tolerancia por defecto (1 %): entre Intel y lavapipe difieren ~0.3 % (bordes de texel y de
        // triángulos)
        const mge_test::GoldenResult result = mge_test::compareWithGolden(golden, image);
        if (!result.message.empty()) MESSAGE(result.message);
        CHECK(result.passed);
    }
}

TEST_CASE("Render: escena del sandbox (SandboxTerrain + atlas PNG + MeshRenderer)") {
    sandboxScene("sandbox_scene", false);
}

TEST_CASE("Render: escena del sandbox con el atlas procedural (sin imágenes, #34)") {
    sandboxScene("sandbox_scene_procedural", true);
}

TEST_CASE("RenderQueue: por pasada; opaco y cutout adelante→atrás, transparente atrás→adelante") {
    // Mallas "no vacías" falsas: la cola solo mira indexCount
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
    queue.add({&empty, glm::mat4(1.0f), nullptr, MeshPass::Opaque, near, {}}); // Vacía: no entra
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
    CHECK(is(4, MeshPass::Transparent, far)); // Lo lejano primero
    CHECK(is(5, MeshPass::Transparent, near));
}

TEST_CASE("Render: hojas (cutout) delante del agua (transparente) y una flor sobre el pasto") {
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
        for (int z = 0; z < 4; z++) world.setBlock({x, 1, z}, b.water); // Charco al fondo
    world.setBlock({5, 2, 5}, b.leaves);                                // Hojas entre la cámara y el agua
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
    // Orden de alta al revés a propósito: la cola lo corrige
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

TEST_CASE("Render: escena voxel de solo color, con luz, AO y niebla (#33)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    // Colinas de colores (sin texturas: solo luz y AO dan la forma)
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
    options.ambientOcclusion = true; // Greedy activo: une las caras con AO uniforme
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

TEST_CASE("Lighting::unlit deja la textura × color tal cual") {
    const Lighting unlit = Lighting::unlit();
    CHECK(unlit.sunColor == glm::vec3(0.0f));
    CHECK(unlit.skyAmbient == glm::vec3(1.0f));
    CHECK(unlit.groundAmbient == glm::vec3(1.0f));
    CHECK_FALSE(unlit.fog);
}

TEST_CASE("Render: detalle procedural en bloques de solo color (MeshMaterial, #34)") {
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
            for (int y = 0; y <= (x + z) / 4; y++) world.setBlock({x, y, z}, s); // Escalones
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}); // Greedy: caras grandes y planas
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

TEST_CASE("DebugLines: cantidad de líneas por figura y esquinas del frustum") {
    DebugLines lines;
    lines.line({0, 0, 0}, {1, 0, 0}, {1, 1, 1, 1});
    CHECK(lines.lineCount() == 1);
    lines.aabb(Aabb({0, 0, 0}, {1, 2, 3}), {1, 0, 0, 1});
    CHECK(lines.lineCount() == 1 + 12);
    lines.aabb(Aabb(), {1, 0, 0, 1}); // Vacía: nada
    CHECK(lines.lineCount() == 13);
    lines.sphere({0, 0, 0}, 1.0f, {0, 1, 0, 1}, 16);
    CHECK(lines.lineCount() == 13 + 3 * 16);
    lines.axes({0, 0, 0}, 2.0f);
    CHECK(lines.lineCount() == 13 + 48 + 3);
    CHECK(lines.vertices().back().position == glm::vec3(0, 0, 2)); // Eje Z
    lines.clear();

    // El frustum de una cámara en el origen mirando a -Z: el plano lejano queda a -far
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

TEST_CASE("Render: líneas de depuración (caja, esfera, ejes) con prueba de profundidad (#38)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();
    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    // Un cubo sólido que tapa parte de las líneas
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
    renderer.draw(cube,
                  glm::translate(glm::mat4(1.0f), {0, -3, 0})); // Mallas después de las líneas: siguen bien
    renderer.end();
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    // Tolerancia por defecto: entre Intel y lavapipe las líneas difieren en ~0.5 % de los píxeles
    const mge_test::GoldenResult result = mge_test::compareWithGolden("debug_lines", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}
