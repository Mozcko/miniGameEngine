#include <doctest/doctest.h>
#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace mge;
namespace fs = std::filesystem;

namespace {
    // Variable de entorno + reinicio de la resolución durante un test
    struct AssetEnv {
        explicit AssetEnv(const char* value) {
            if (value)
                setenv("MGE_ASSET_DIR", value, 1);
            else
                unsetenv("MGE_ASSET_DIR");
            setAssetRoot({}); // Forzar una nueva búsqueda
            Log::setSink([](const LogRecord&) {});
        }
        ~AssetEnv() {
            unsetenv("MGE_ASSET_DIR");
            setAssetRoot({});
            Log::setSink(nullptr);
        }
    };

    fs::path makeTempDir(const std::string& name) {
        fs::path dir = fs::temp_directory_path() / ("mge_test_" + name);
        fs::create_directories(dir);
        return dir;
    }
}

TEST_CASE("executableDir es absoluta y existe") {
    fs::path dir = executableDir();
    CHECK(dir.is_absolute());
    CHECK(fs::is_directory(dir));
}

TEST_CASE("Sin MGE_ASSET_DIR se encuentra assets/ del proyecto (desde cualquier cwd)") {
    AssetEnv env(nullptr);
    fs::path root = assetRoot();
    CHECK(fs::is_directory(root));
    CHECK(root.filename() == "assets");
    CHECK(fs::exists(assetPath("shaders/basic3d.vert")));
}

TEST_CASE("MGE_ASSET_DIR tiene prioridad") {
    fs::path dir = makeTempDir("assets_env");
    AssetEnv env(dir.string().c_str());
    CHECK(fs::equivalent(assetRoot(), dir));
    CHECK(assetPath("a/b.txt") == assetRoot() / "a" / "b.txt");
    fs::remove_all(dir);
}

TEST_CASE("Un MGE_ASSET_DIR que no existe se ignora") {
    AssetEnv env("/no/existe/mge_assets");
    CHECK(fs::is_directory(assetRoot()));
    CHECK(assetRoot().filename() == "assets");
}

TEST_CASE("setAssetRoot fija la raíz a mano y vacío vuelve a buscar") {
    AssetEnv env(nullptr);
    setAssetRoot("/opt/mi_juego/data");
    CHECK(assetRoot() == fs::path("/opt/mi_juego/data"));
    CHECK(assetPath("x.png") == fs::path("/opt/mi_juego/data/x.png"));
    setAssetRoot({});
    CHECK(assetRoot().filename() == "assets");
}
