// Galería procedural: genera mallas con mge_procgen y las exporta a OBJ en <build>/gallery/
// para verlas en Blender o cualquier visor OBJ. Headless (sin ventana ni GPU); también es un
// smoke test: sale con código 1 si alguna malla es inválida o no se pudo escribir.
#include <mge/mge.h>

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
