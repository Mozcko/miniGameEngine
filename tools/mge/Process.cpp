#include "Process.h"

#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace mge::cli {

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

    std::filesystem::path getExecutablePath() {
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
        return std::filesystem::current_path() / "mge";
    }

} // namespace mge::cli
