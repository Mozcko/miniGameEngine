#include <doctest/doctest.h>
#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace mge;
namespace fs = std::filesystem;

namespace {
    // Environment variable + resolution reset during a test
    struct AssetEnv {
        explicit AssetEnv(const char* value) {
            if (value)
                setenv("MGE_ASSET_DIR", value, 1);
            else
                unsetenv("MGE_ASSET_DIR");
            setAssetRoot({}); // Force a fresh search
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

TEST_CASE("executableDir is absolute and exists") {
    fs::path dir = executableDir();
    CHECK(dir.is_absolute());
    CHECK(fs::is_directory(dir));
}

TEST_CASE("Without MGE_ASSET_DIR, project assets/ is found (from any cwd)") {
    AssetEnv env(nullptr);
    fs::path root = assetRoot();
    CHECK(fs::is_directory(root));
    CHECK(root.filename() == "assets");
    CHECK(fs::exists(assetPath("textures/Minecraft-like_3d_tileset.png")));
}

TEST_CASE("MGE_ASSET_DIR takes priority") {
    fs::path dir = makeTempDir("assets_env");
    AssetEnv env(dir.string().c_str());
    CHECK(fs::equivalent(assetRoot(), dir));
    CHECK(assetPath("a/b.txt") == assetRoot() / "a" / "b.txt");
    fs::remove_all(dir);
}

TEST_CASE("A nonexistent MGE_ASSET_DIR is ignored") {
    AssetEnv env("/does/not/exist/mge_assets");
    CHECK(fs::is_directory(assetRoot()));
    CHECK(assetRoot().filename() == "assets");
}

TEST_CASE("setAssetRoot sets the root manually and empty triggers search again") {
    AssetEnv env(nullptr);
    setAssetRoot("/opt/my_game/data");
    CHECK(assetRoot() == fs::path("/opt/my_game/data"));
    CHECK(assetPath("x.png") == fs::path("/opt/my_game/data/x.png"));
    setAssetRoot({});
    CHECK(assetRoot().filename() == "assets");
}

TEST_CASE("setAssetRoot with relative path prefixes exactly once") {
    AssetEnv env(nullptr);
    setAssetRoot("relative_root");
    CHECK(assetRoot() == fs::path("relative_root"));
    const fs::path p = assetPath("textures/Minecraft-like_3d_tileset.png");
    CHECK(p == fs::path("relative_root") / "textures" / "Minecraft-like_3d_tileset.png");
    setAssetRoot({});
}

TEST_CASE("Without assets folder or MGE_ASSET_DIR, no WARN or ERROR logs are emitted") {
    bool hasWarnOrError = false;
    Log::setSink([&](const LogRecord& record) {
        if (record.category == "Assets" &&
            (record.level == LogLevel::Warn || record.level == LogLevel::Error)) {
            hasWarnOrError = true;
        }
    });
    setAssetRoot(fs::path("/nonexistent/directory/assets"));
    const fs::path p = assetPath("file.png");
    CHECK(p == fs::path("/nonexistent/directory/assets/file.png"));
    CHECK_FALSE(hasWarnOrError);
    setAssetRoot({});
    Log::setSink(nullptr);
}
