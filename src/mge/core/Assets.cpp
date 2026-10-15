#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <cstdlib>
#include <mutex>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#endif

namespace mge {

    namespace fs = std::filesystem;

    namespace {
        struct AssetState {
            std::mutex mutex;
            fs::path root;
            bool resolved = false;
        };

        AssetState& state() {
            static AssetState s;
            return s;
        }

        bool isDirectory(const fs::path& path) {
            std::error_code error;
            return !path.empty() && fs::is_directory(path, error);
        }

        fs::path normalized(const fs::path& path) {
            std::error_code error;
            fs::path canonical = fs::weakly_canonical(path, error);
            return error ? path : canonical;
        }

        fs::path resolveRoot() {
            if (const char* env = std::getenv("MGE_ASSET_DIR"); env && *env) {
                if (isDirectory(env)) {
                    MGE_LOG_INFO("Assets", "Raíz de assets (MGE_ASSET_DIR): ", normalized(env).string());
                    return normalized(env);
                }
                MGE_LOG_WARN("Assets", "MGE_ASSET_DIR='", env, "' no es una carpeta; se ignora");
            }

            const fs::path exeDir = executableDir();
            for (const fs::path& candidate : {exeDir / "assets", exeDir / ".." / "assets"}) {
                if (!exeDir.empty() && isDirectory(candidate)) {
                    MGE_LOG_INFO("Assets",
                                 "Raíz de assets (junto al ejecutable): ", normalized(candidate).string());
                    return normalized(candidate);
                }
            }

#ifdef MGE_SOURCE_ASSET_DIR
            if (isDirectory(MGE_SOURCE_ASSET_DIR)) {
                MGE_LOG_INFO("Assets", "Raíz de assets (código fuente): ", MGE_SOURCE_ASSET_DIR);
                return fs::path(MGE_SOURCE_ASSET_DIR);
            }
#endif
            MGE_LOG_WARN("Assets",
                         "No se encontró la carpeta de assets; usando 'assets' relativo al directorio "
                         "actual. Definir MGE_ASSET_DIR para indicarla.");
            return fs::path("assets");
        }
    }

    fs::path executableDir() {
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH];
        DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length == 0 || length == MAX_PATH) return {};
        return fs::path(buffer).parent_path();
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
        return normalized(buffer.data()).parent_path();
#else
        std::error_code error;
        fs::path exe = fs::read_symlink("/proc/self/exe", error);
        return error ? fs::path() : exe.parent_path();
#endif
    }

    const fs::path& assetRoot() {
        AssetState& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.resolved) {
            s.root = resolveRoot();
            s.resolved = true;
        }
        return s.root;
    }

    void setAssetRoot(const fs::path& root) {
        AssetState& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.root = root;
        s.resolved = !root.empty();
    }

    fs::path assetPath(std::string_view relative) {
        return assetRoot() / fs::path(relative);
    }
}
