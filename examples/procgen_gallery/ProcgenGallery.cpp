// Galería procedural: genera mallas con mge_procgen y las exporta a OBJ en <build>/gallery/
// para verlas en Blender o cualquier visor OBJ. Headless (sin ventana ni GPU); también es un
// smoke test: sale con código 1 si alguna malla es inválida o no se pudo escribir.
#include <mge/mge.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
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
            MGE_LOG_INFO("Gallery", written, " mallas exportadas a ", outDir.string());
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
