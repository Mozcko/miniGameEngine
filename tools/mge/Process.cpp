#include "Process.h"

#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <vector>

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mge::cli {

#if !defined(_WIN32)
    int runCommand(const std::vector<std::string>& args, const std::filesystem::path& workingDir) {
        if (args.empty()) {
            return -1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            return -1;
        }

        if (pid == 0) {
            // Proceso hijo: cambia de directorio y ejecuta el binario sin invocación a shell
            if (!workingDir.empty()) {
                if (chdir(workingDir.c_str()) != 0) {
                    _exit(127);
                }
            }

            std::vector<char*> cArgs;
            cArgs.reserve(args.size() + 1);
            for (const auto& arg : args) {
                cArgs.push_back(const_cast<char*>(arg.c_str()));
            }
            cArgs.push_back(nullptr);

            execvp(cArgs[0], cArgs.data());
            _exit(127);
        }

        // Proceso padre: espera que termine el hijo y extrae el código de salida
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            return -1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return -1;
    }

    int runCommandCapture(const std::vector<std::string>& args, std::string& outOutput,
                          const std::filesystem::path& workingDir) {
        outOutput.clear();
        if (args.empty()) {
            return -1;
        }

        int pipefd[2];
        if (pipe(pipefd) != 0) {
            return -1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return -1;
        }

        if (pid == 0) {
            // Proceso hijo: redirige stdout y stderr hacia el descriptor de escritura del pipe
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            if (!workingDir.empty()) {
                if (chdir(workingDir.c_str()) != 0) {
                    _exit(127);
                }
            }

            std::vector<char*> cArgs;
            cArgs.reserve(args.size() + 1);
            for (const auto& arg : args) {
                cArgs.push_back(const_cast<char*>(arg.c_str()));
            }
            cArgs.push_back(nullptr);

            execvp(cArgs[0], cArgs.data());
            _exit(127);
        }

        // Proceso padre: lee de manera continua la salida del proceso hijo
        close(pipefd[1]);

        std::array<char, 512> buffer{};
        ssize_t bytesRead = 0;
        while ((bytesRead = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
            outOutput.append(buffer.data(), static_cast<size_t>(bytesRead));
        }
        close(pipefd[0]);

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            return -1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return -1;
    }

    bool commandExists(const std::string& commandName) {
        if (commandName.empty()) {
            return false;
        }

        // Si contiene barra '/', verificar ruta directa
        if (commandName.find('/') != std::string::npos) {
            return access(commandName.c_str(), X_OK) == 0;
        }

        // Buscar en las rutas del PATH del sistema
        const char* pathEnv = std::getenv("PATH");
        if (pathEnv == nullptr) {
            pathEnv = "/usr/bin:/bin";
        }

        std::stringstream ss(pathEnv);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) {
                continue;
            }
            std::filesystem::path fullPath = std::filesystem::path(dir) / commandName;
            if (access(fullPath.c_str(), X_OK) == 0) {
                return true;
            }
        }

        return false;
    }
#else
    int runCommand(const std::vector<std::string>& /*args*/, const std::filesystem::path& /*workingDir*/) {
        return -1;
    }

    int runCommandCapture(const std::vector<std::string>& /*args*/, std::string& /*outOutput*/,
                          const std::filesystem::path& /*workingDir*/) {
        return -1;
    }

    bool commandExists(const std::string& /*commandName*/) {
        return false;
    }
#endif

    std::filesystem::path getExecutablePath() {
#if defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        if (size > 0) {
            std::vector<char> buffer(size);
            if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
                std::error_code ec;
                auto canonical = std::filesystem::canonical(buffer.data(), ec);
                if (!ec) {
                    return canonical;
                }
                return std::filesystem::path(buffer.data());
            }
        }
#elif defined(__linux__)
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            std::error_code ec;
            auto canonical = std::filesystem::canonical(buffer, ec);
            if (!ec) {
                return canonical;
            }
            return std::filesystem::path(buffer);
        }
#elif defined(_WIN32)
        wchar_t buffer[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (len > 0) {
            return std::filesystem::path(buffer);
        }
#endif
        return std::filesystem::current_path() / "mge";
    }

} // namespace mge::cli
