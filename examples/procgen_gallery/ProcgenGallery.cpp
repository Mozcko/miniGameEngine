// Galería procedural: genera mallas con mge_procgen (y mge_voxel) y las exporta a OBJ en <build>/gallery/
// para verlas en Blender o cualquier visor OBJ. Headless (sin ventana ni GPU); también es un
// smoke test: sale con código 1 si alguna malla es inválida o no se pudo escribir.
#include <mge/mge.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    // Un quad de colores: la malla más simple, para probar MeshData + OBJ
    mge::MeshData coloredQuad() {
        mge::MeshData mesh;
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        auto a = mesh.addVertex({-1, 0, 1}, up, {1, 0, 0, 1}, {0, 0});
        auto b = mesh.addVertex({1, 0, 1}, up, {0, 1, 0, 1}, {1, 0});
        auto c = mesh.addVertex({1, 0, -1}, up, {0, 0, 1, 1}, {1, 1});
        auto d = mesh.addVertex({-1, 0, -1}, up, {1, 1, 0, 1}, {0, 1});
        mesh.addQuad(a, b, c, d);
        return mesh;
    }

    // Estrella de 5 puntas (polígono cóncavo) para extruir
    std::vector<glm::vec2> star() {
        std::vector<glm::vec2> points;
        for (int i = 0; i < 10; i++) {
            const float angle = static_cast<float>(i) * 3.14159265f / 5.0f + 1.5707963f;
            const float radius = i % 2 == 0 ? 1.0f : 0.45f;
            points.push_back({radius * std::cos(angle), radius * std::sin(angle)});
        }
        return points;
    }

    // Operaciones de malla (#21)
    void addMeshOps(std::vector<std::pair<std::string, mge::MeshData>>& meshes) {
        meshes.emplace_back("op_extrude_star", mge::extrude(star(), 0.4f, {0.95f, 0.8f, 0.2f, 1.0f}));

        // Florero por torno: perfil de abajo hacia arriba, cerrado en la base
        meshes.emplace_back(
            "op_lathe_vase",
            mge::lathe({{0.0f, 0.0f}, {0.5f, 0.0f}, {0.7f, 0.4f}, {0.4f, 1.2f}, {0.35f, 1.6f}, {0.5f, 1.9f}},
                       48, {0.3f, 0.6f, 0.9f, 1.0f}));

        // Resorte: un círculo barrido por una hélice
        std::vector<glm::vec2> circle;
        for (int i = 0; i < 12; i++) {
            const float a = static_cast<float>(i) / 12.0f * 6.2831853f;
            circle.push_back({0.12f * std::cos(a), 0.12f * std::sin(a)});
        }
        std::vector<glm::vec3> helix;
        for (int i = 0; i <= 200; i++) {
            const float t = static_cast<float>(i) / 200.0f * 6.2831853f * 4.0f;
            helix.push_back({std::cos(t), t * 0.08f, std::sin(t)});
        }
        meshes.emplace_back("op_sweep_spring",
                            mge::sweep(circle, helix, false, true, {0.8f, 0.3f, 0.3f, 1.0f}));

        // Icosfera facetada (normales planas)
        mge::MeshData faceted = mge::primitives::icosphere(1.0f, 1, {0.7f, 0.9f, 0.5f, 1.0f});
        mge::recomputeNormals(faceted, mge::NormalMode::Flat);
        meshes.emplace_back("op_flat_icosphere", faceted);

        // Escena combinada: transformar y unir primitivas en una sola malla
        mge::MeshData scene = mge::primitives::plane(6.0f, 6.0f, 1, 1, {0.4f, 0.4f, 0.45f, 1.0f});
        mge::MeshData column = mge::primitives::cylinder(0.3f, 2.0f, 24, true, {0.9f, 0.9f, 0.85f, 1.0f});
        for (int i = 0; i < 4; i++) {
            mge::MeshData copy = column;
            mge::transformMesh(
                copy, glm::translate(glm::mat4(1.0f), {i % 2 ? 2.0f : -2.0f, 1.0f, i < 2 ? 2.0f : -2.0f}));
            mge::appendMesh(scene, copy);
        }
        meshes.emplace_back("op_merged_scene", scene);
    }

    // Ruido (#22): terreno = plano subdividido desplazado por fbm, coloreado por altura
    mge::MeshData noiseTerrain() {
        const mge::Noise noise(2024);
        mge::FractalSettings settings;
        settings.octaves = 6;
        settings.frequency = 0.35f;
        mge::MeshData mesh = mge::primitives::plane(12.0f, 12.0f, 160, 160);
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            glm::vec3& p = mesh.positions[i];
            const float h = noise.fbm(mge::NoiseType::Simplex, p.x, p.z, settings);
            p.y = h * 2.0f;
            // Agua, arena, pasto, roca, nieve
            if (h < -0.2f)
                mesh.colors[i] = {0.15f, 0.35f, 0.75f, 1.0f};
            else if (h < -0.1f)
                mesh.colors[i] = {0.85f, 0.8f, 0.55f, 1.0f};
            else if (h < 0.35f)
                mesh.colors[i] = {0.25f, 0.6f, 0.25f, 1.0f};
            else if (h < 0.6f)
                mesh.colors[i] = {0.5f, 0.45f, 0.4f, 1.0f};
            else
                mesh.colors[i] = {0.95f, 0.95f, 0.97f, 1.0f};
        }
        mge::recomputeNormals(mesh, mge::NormalMode::Smooth);
        return mesh;
    }

    // SDF + surface nets (#23)
    void addSdfShapes(std::vector<std::pair<std::string, mge::MeshData>>& meshes) {
        using namespace mge::sdf;
        const mge::Aabb bounds({-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f});

        Sdf blob =
            opSmoothUnion(opSmoothUnion(sphere(0.8f), opTranslate(sphere(0.55f), {0.9f, 0.4f, 0.0f}), 0.35f),
                          opTranslate(sphere(0.5f), {-0.6f, 0.7f, 0.5f}), 0.35f);
        meshes.emplace_back("sdf_blob", surfaceNets(blob, bounds, 80, {0.9f, 0.45f, 0.6f, 1.0f}));

        meshes.emplace_back("sdf_twisted_box", surfaceNets(opTwist(box({0.6f, 1.6f, 0.6f}), 1.2f), bounds, 80,
                                                           {0.4f, 0.8f, 0.9f, 1.0f}));

        Sdf carved = opSubtract(roundBox({1.0f, 1.0f, 1.0f}, 0.15f), sphere(1.25f));
        meshes.emplace_back("sdf_carved_box", surfaceNets(carved, bounds, 80, {0.85f, 0.75f, 0.55f, 1.0f}));

        Sdf grid = opIntersect(opRepeat(sphere(0.3f), {0.9f, 0.9f, 0.9f}), box({1.6f, 1.6f, 1.6f}));
        meshes.emplace_back("sdf_repeated_spheres",
                            surfaceNets(grid, bounds, 96, {0.95f, 0.85f, 0.3f, 1.0f}));

        Sdf table =
            opUnion(opTranslate(roundBox({1.4f, 0.08f, 0.9f}, 0.05f), {0.0f, 0.8f, 0.0f}),
                    opRepeat(opTranslate(cylinder(0.08f, 0.8f), {0.0f, 0.0f, 0.0f}), {2.4f, 0.0f, 1.4f}));
        meshes.emplace_back("sdf_table", surfaceNets(opIntersect(table, box({1.6f, 1.0f, 1.0f})), bounds, 96,
                                                     {0.6f, 0.4f, 0.25f, 1.0f}));
    }

    // Voxels (#25, #26): un mundo de 4×2×4 chunks solo con colores (sin texturas), mallado con greedy + AO.
    // Es el camino procedural de mge_voxel; el sandbox usa el otro (atlas de texturas).
    struct VoxelBlocks {
        mge::BlockRegistry registry;
        mge::BlockId grass, dirt, stone, sand, water, snow;

        static mge::BlockDesc colored(const char* name, glm::vec4 color, bool opaque = true) {
            mge::BlockDesc desc;
            desc.name = name;
            desc.color = color;
            desc.opaque = opaque;
            desc.solid = opaque;
            return desc;
        }

        VoxelBlocks() {
            grass = registry.add(colored("grass", {0.30f, 0.62f, 0.25f, 1.0f}));
            dirt = registry.add(colored("dirt", {0.47f, 0.33f, 0.21f, 1.0f}));
            stone = registry.add(colored("stone", {0.52f, 0.52f, 0.55f, 1.0f}));
            sand = registry.add(colored("sand", {0.86f, 0.80f, 0.56f, 1.0f}));
            water = registry.add(colored("water", {0.20f, 0.42f, 0.85f, 0.7f}, false));
            snow = registry.add(colored("snow", {0.95f, 0.95f, 0.97f, 1.0f}));
        }
    };

    class ColorTerrain : public mge::TerrainGenerator {
    public:
        explicit ColorTerrain(const VoxelBlocks& b) : m_b(b), m_noise(7) {}

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override {
            mge::FractalSettings settings;
            settings.octaves = 4;
            settings.frequency = 0.02f;
            const glm::ivec3 origin = mge::chunkOrigin(coord);
            for (int x = 0; x < mge::kChunkSize; x++) {
                for (int z = 0; z < mge::kChunkSize; z++) {
                    const float n = m_noise.fbm(mge::NoiseType::Simplex, float(origin.x + x),
                                                float(origin.z + z), settings);
                    const int height = 10 + static_cast<int>(n * 14.0f);
                    for (int y = 0; y < mge::kChunkSize; y++) {
                        const int wy = origin.y + y;
                        mge::BlockId id = mge::kAir;
                        if (wy > height)
                            id = wy <= kWaterLevel ? m_b.water : mge::kAir;
                        else if (wy < height - 3)
                            id = m_b.stone;
                        else if (wy < height)
                            id = m_b.dirt;
                        else
                            id = height <= kWaterLevel + 1 ? m_b.sand : height > 19 ? m_b.snow : m_b.grass;
                        chunk.set(x, y, z, id);
                    }
                }
            }
        }

    private:
        static constexpr int kWaterLevel = 7;
        const VoxelBlocks& m_b;
        mge::Noise m_noise;
    };

    void addVoxelTerrain(std::vector<std::pair<std::string, mge::MeshData>>& meshes) {
        const VoxelBlocks blocks;
        mge::VoxelWorld world(blocks.registry, std::make_unique<ColorTerrain>(blocks));
        for (int x = 0; x < 4; x++)
            for (int y = 0; y < 2; y++)
                for (int z = 0; z < 4; z++) world.loadChunk({x, y, z});

        mge::ChunkMeshOptions options;
        options.ambientOcclusion = true;
        mge::MeshData opaque, transparent;
        std::size_t naiveTriangles = 0;
        mge::ChunkMeshOptions naive;
        naive.greedy = false;
        for (const mge::ChunkCoord& coord : world.loadedChunks()) {
            mge::ChunkMesh mesh = mge::buildChunkMesh(world, coord, options);
            const glm::mat4 toWorld = glm::translate(glm::mat4(1.0f), glm::vec3(mge::chunkOrigin(coord)));
            mge::transformMesh(mesh.opaque, toWorld);
            mge::transformMesh(mesh.transparent, toWorld);
            mge::appendMesh(opaque, mesh.opaque);
            mge::appendMesh(transparent, mesh.transparent);
            const mge::ChunkMesh plain = mge::buildChunkMesh(world, coord, naive);
            naiveTriangles += plain.opaque.triangleCount() + plain.transparent.triangleCount();
        }
        MGE_LOG_INFO("Gallery", "voxels: ", opaque.triangleCount() + transparent.triangleCount(),
                     " triángulos con greedy, ", naiveTriangles, " sin greedy");
        // Escala 1/8 para verlo del mismo tamaño que el resto de la galería
        const glm::mat4 shrink = glm::scale(glm::mat4(1.0f), glm::vec3(0.125f));
        mge::transformMesh(opaque, shrink);
        mge::transformMesh(transparent, shrink);
        meshes.emplace_back("voxel_terrain", std::move(opaque));
        meshes.emplace_back("voxel_water", std::move(transparent));
    }

    // Generadores de texturas (#34): una muestra de cada uno en una imagen de 4×2 tiles de 64 px
    bool writeTextureSamples(const std::filesystem::path& path) {
        namespace tg = mge::texgen;
        constexpr int kTile = 64;
        mge::Image image(4 * kTile, 2 * kTile);
        const tg::Palette stone({0.38f, 0.38f, 0.40f, 1}, {0.64f, 0.64f, 0.66f, 1});
        const tg::Palette wood({0.55f, 0.40f, 0.22f, 1}, {0.78f, 0.62f, 0.38f, 1});
        const tg::Palette brick({0.50f, 0.20f, 0.14f, 1}, {0.72f, 0.34f, 0.24f, 1});
        auto tile = [](int i) { return tg::tileRect(i, 4, kTile); };
        tg::noise(image, tile(0),
                  tg::Palette({{0.0f, {0.1f, 0.3f, 0.7f, 1}},
                               {0.5f, {0.3f, 0.6f, 0.3f, 1}},
                               {1.0f, {0.9f, 0.9f, 0.9f, 1}}}),
                  4.0f, 1, 5);
        tg::noise(image, tile(1), stone, 6.0f, 2);
        tg::speckle(image, tile(1), {0.2f, 0.2f, 0.2f, 1}, 0.05f, 3);
        tg::bricks(image, tile(2), brick, {0.75f, 0.73f, 0.70f, 1}, 6, 3, 2, 4);
        tg::planks(image, tile(3), wood, 5, 5);
        tg::noise(image, tile(4), stone, 6.0f, 6);
        tg::ore(image, tile(4), {0.95f, 0.75f, 0.20f, 1}, 6, 4, 7);
        tg::stripes(image, tile(5), tg::Palette({0.25f, 0.17f, 0.09f, 1}, {0.45f, 0.32f, 0.18f, 1}), 5, 8);
        tg::rings(image, tile(6), wood, 5.0f, 9);
        tg::plant(image, tile(7), {0.2f, 0.5f, 0.15f, 1}, {0.9f, 0.2f, 0.5f, 1}, 9, 10);
        return mge::writePng(image, path);
    }

    // Atlas de la fuente embebida (#41), ampliado ×4 sobre fondo oscuro para verlo en cualquier visor
    bool writeFontSample(const std::filesystem::path& path) {
        const mge::Font font = mge::Font::builtin();
        const mge::Image& atlas = font.atlas();
        constexpr int kZoom = 4;
        mge::Image image(atlas.width() * kZoom, atlas.height() * kZoom, {0.1f, 0.1f, 0.12f, 1.0f});
        for (int y = 0; y < int(image.height()); y++)
            for (int x = 0; x < int(image.width()); x++)
                if (atlas.get(x / kZoom, y / kZoom).a > 0.5f) image.set(x, y, {1.0f, 0.9f, 0.3f, 1.0f});
        return mge::writePng(image, path);
    }

    class ProcgenGallery : public mge::Application {
    public:
        ProcgenGallery() : mge::Application(makeConfig()) {}

    protected:
        bool onInit() override {
            const std::filesystem::path outDir = mge::executableDir() / "gallery";
            std::filesystem::create_directories(outDir);

            std::vector<std::pair<std::string, mge::MeshData>> meshes;
            meshes.emplace_back("quad", coloredQuad());

            // Primitivas (#20)
            namespace prim = mge::primitives;
            meshes.emplace_back("plane", prim::plane(4.0f, 4.0f, 8, 8, {0.4f, 0.8f, 0.3f, 1.0f}));
            meshes.emplace_back("box", prim::box({1.0f, 2.0f, 1.5f}, {0.9f, 0.5f, 0.2f, 1.0f}));
            meshes.emplace_back("uv_sphere", prim::uvSphere(1.0f, 32, 16, {0.3f, 0.5f, 0.9f, 1.0f}));
            meshes.emplace_back("icosphere", prim::icosphere(1.0f, 3, {0.8f, 0.8f, 0.9f, 1.0f}));
            meshes.emplace_back("cylinder", prim::cylinder(0.7f, 2.0f, 32, true, {0.9f, 0.2f, 0.3f, 1.0f}));
            meshes.emplace_back("cone", prim::cone(0.8f, 2.0f, 32, {0.9f, 0.8f, 0.2f, 1.0f}));
            meshes.emplace_back("torus", prim::torus(1.2f, 0.4f, 48, 24, {0.6f, 0.3f, 0.8f, 1.0f}));
            addMeshOps(meshes);
            meshes.emplace_back("noise_terrain", noiseTerrain());
            addSdfShapes(meshes);
            addVoxelTerrain(meshes);
            meshes.emplace_back("capsule", prim::capsule(0.5f, 1.5f, 32, 8, {0.2f, 0.8f, 0.7f, 1.0f}));

            int written = 0;
            for (const auto& [name, mesh] : meshes) {
                const std::string error = mesh.validate();
                if (!error.empty()) {
                    MGE_LOG_ERROR("Gallery", name, ": malla inválida: ", error);
                    setExitCode(1);
                    continue;
                }
                if (mge::writeObj(mesh, outDir / (name + ".obj"))) {
                    written++;
                } else {
                    setExitCode(1);
                }
            }
            if (!writeTextureSamples(outDir / "texture_generators.png")) setExitCode(1);
            if (!writeFontSample(outDir / "font_builtin.png")) setExitCode(1);
            MGE_LOG_INFO("Gallery", written,
                         " mallas, texture_generators.png y font_builtin.png exportados a ", outDir.string());
            requestStop();
            return true;
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "procgen_gallery";
            config.headless = true;
            return config;
        }
    };
}

mge::Application* mge::createApplication() {
    return new ProcgenGallery();
}
