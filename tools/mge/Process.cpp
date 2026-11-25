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
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
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
            // Child process: change directory and execute binary without invoking a shell
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

        // Parent process: wait for child to finish and extract exit code
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
            // Child process: redirect stdout and stderr to the pipe write descriptor
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

        // Parent process: continuously read output from the child process
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

        // If it contains a slash '/', check direct path
        if (commandName.find('/') != std::string::npos) {
            return access(commandName.c_str(), X_OK) == 0;
        }

        // Search in system PATH directories
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

    bool isStdinTty() {
        return isatty(STDIN_FILENO) != 0;
    }

    bool isStdoutTty() {
        return isatty(STDOUT_FILENO) != 0;
    }

    bool enableTerminalColors() {
        return true;
    }

    void initTerminal() {}
#else
    namespace {

        std::wstring utf8ToWide(const std::string& str) {
            if (str.empty()) {
                return {};
            }
            int needed =
                MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            if (needed <= 0) {
                return {};
            }
            std::wstring result(static_cast<size_t>(needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), needed);
            return result;
        }

        std::string quoteWindowsArg(const std::string& arg) {
            if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) {
                return arg;
            }
            std::string quoted;
            quoted.push_back('"');
            for (size_t i = 0; i < arg.size(); ++i) {
                size_t numBackslashes = 0;
                while (i < arg.size() && arg[i] == '\\') {
                    ++numBackslashes;
                    ++i;
                }
                if (i == arg.size()) {
                    quoted.append(numBackslashes * 2, '\\');
                    break;
                }
                if (arg[i] == '"') {
                    quoted.append(numBackslashes * 2 + 1, '\\');
                    quoted.push_back('"');
                } else {
                    quoted.append(numBackslashes, '\\');
                    quoted.push_back(arg[i]);
                }
            }
            quoted.push_back('"');
            return quoted;
        }

        bool isBatchScript(const std::filesystem::path& path) {
            std::string ext = path.extension().string();
            for (char& ch : ext) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return ext == ".bat" || ext == ".cmd";
        }

        std::vector<std::wstring> getPathExts() {
            std::vector<std::wstring> exts;
            const char* pathextEnv = std::getenv("PATHEXT");
            std::string pathext =
                (pathextEnv != nullptr && pathextEnv[0] != '\0') ? pathextEnv : ".COM;.EXE;.BAT;.CMD";
            std::stringstream ss(pathext);
            std::string item;
            while (std::getline(ss, item, ';')) {
                if (!item.empty()) {
                    exts.push_back(utf8ToWide(item));
                }
            }
            return exts;
        }

        std::filesystem::path resolveWindowsCommand(const std::string& commandName) {
            if (commandName.empty()) {
                return {};
            }

            const bool hasDirSep = commandName.find('/') != std::string::npos ||
                                   commandName.find('\\') != std::string::npos ||
                                   commandName.find(':') != std::string::npos;
            std::wstring wideCmd = utf8ToWide(commandName);
            if (wideCmd.empty()) {
                return {};
            }

            auto isRegularFile = [](const std::filesystem::path& p) {
                std::error_code ec;
                return std::filesystem::is_regular_file(p, ec);
            };

            if (hasDirSep) {
                std::filesystem::path direct(wideCmd);
                if (isRegularFile(direct)) {
                    return direct;
                }
                if (!direct.has_extension()) {
                    for (const auto& ext : getPathExts()) {
                        std::filesystem::path candidate = direct;
                        candidate += ext;
                        if (isRegularFile(candidate)) {
                            return candidate;
                        }
                    }
                }
                return {};
            }

            auto searchWithExt = [&](const wchar_t* ext) -> std::filesystem::path {
                DWORD needed = SearchPathW(nullptr, wideCmd.c_str(), ext, 0, nullptr, nullptr);
                if (needed == 0) {
                    return {};
                }
                std::wstring buf(static_cast<size_t>(needed), L'\0');
                DWORD written = SearchPathW(nullptr, wideCmd.c_str(), ext, needed, buf.data(), nullptr);
                if (written == 0 || written >= needed) {
                    return {};
                }
                buf.resize(static_cast<size_t>(written));
                std::filesystem::path found(buf);
                return isRegularFile(found) ? found : std::filesystem::path{};
            };

            std::filesystem::path directMatch = searchWithExt(nullptr);
            if (!directMatch.empty()) {
                return directMatch;
            }
            for (const auto& ext : getPathExts()) {
                std::filesystem::path match = searchWithExt(ext.c_str());
                if (!match.empty()) {
                    return match;
                }
            }
            return {};
        }

        int runWindowsProcess(const std::vector<std::string>& args, const std::filesystem::path& workingDir,
                              std::string* capturedOutput) {
            if (args.empty()) {
                return -1;
            }

            std::filesystem::path resolved = resolveWindowsCommand(args[0]);
            if (resolved.empty()) {
                return 127;
            }

            std::wstring appName;
            std::wstring cmdLine;
            if (isBatchScript(resolved)) {
                std::filesystem::path cmdExe = resolveWindowsCommand("cmd.exe");
                appName = cmdExe.empty() ? L"cmd.exe" : cmdExe.wstring();
                std::string inner = quoteWindowsArg(resolved.string());
                for (size_t i = 1; i < args.size(); ++i) {
                    inner.push_back(' ');
                    inner += quoteWindowsArg(args[i]);
                }
                cmdLine = utf8ToWide("cmd.exe /d /s /c \"" + inner + "\"");
            } else {
                appName = resolved.wstring();
                std::string joined = quoteWindowsArg(args[0]);
                for (size_t i = 1; i < args.size(); ++i) {
                    joined.push_back(' ');
                    joined += quoteWindowsArg(args[i]);
                }
                cmdLine = utf8ToWide(joined);
            }

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            BOOL inheritHandles = FALSE;

            if (capturedOutput != nullptr) {
                SECURITY_ATTRIBUTES sa{};
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;
                sa.lpSecurityDescriptor = nullptr;

                if (CreatePipe(&readPipe, &writePipe, &sa, 0) == 0) {
                    return -1;
                }
                SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                si.hStdOutput = writePipe;
                si.hStdError = writePipe;
                inheritHandles = TRUE;
            }

            std::wstring wideWorkDir;
            const wchar_t* workDirPtr = nullptr;
            if (!workingDir.empty()) {
                wideWorkDir = workingDir.wstring();
                workDirPtr = wideWorkDir.c_str();
            }

            // CreateProcessW requires a mutable null-terminated command-line buffer
            std::vector<wchar_t> mutableCmdLine(cmdLine.begin(), cmdLine.end());
            mutableCmdLine.push_back(L'\0');

            PROCESS_INFORMATION pi{};
            BOOL created = CreateProcessW(appName.c_str(), mutableCmdLine.data(), nullptr, nullptr,
                                          inheritHandles, 0, nullptr, workDirPtr, &si, &pi);

            if (writePipe != nullptr) {
                CloseHandle(writePipe);
                writePipe = nullptr;
            }

            if (created == 0) {
                if (readPipe != nullptr) {
                    CloseHandle(readPipe);
                }
                return -1;
            }

            if (capturedOutput != nullptr && readPipe != nullptr) {
                std::array<char, 512> buffer{};
                DWORD bytesRead = 0;
                while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead,
                                nullptr) != 0 &&
                       bytesRead > 0) {
                    capturedOutput->append(buffer.data(), static_cast<size_t>(bytesRead));
                }
                CloseHandle(readPipe);
                readPipe = nullptr;
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 1;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) == 0) {
                exitCode = 1;
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return static_cast<int>(exitCode);
        }

    } // namespace

    int runCommand(const std::vector<std::string>& args, const std::filesystem::path& workingDir) {
        return runWindowsProcess(args, workingDir, nullptr);
    }

    int runCommandCapture(const std::vector<std::string>& args, std::string& outOutput,
                          const std::filesystem::path& workingDir) {
        outOutput.clear();
        return runWindowsProcess(args, workingDir, &outOutput);
    }

    bool commandExists(const std::string& commandName) {
        return !resolveWindowsCommand(commandName).empty();
    }

    bool isStdinTty() {
        return _isatty(_fileno(stdin)) != 0;
    }

    bool isStdoutTty() {
        return _isatty(_fileno(stdout)) != 0;
    }

    bool enableTerminalColors() {
        static bool initialized = false;
        static bool enabled = false;
        if (!initialized) {
            initialized = true;
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
                DWORD mode = 0;
                if (GetConsoleMode(hOut, &mode) != 0) {
                    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
                        enabled = true;
                    } else if (SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
                        enabled = true;
                    }
                }
            }
        }
        return enabled;
    }

    void initTerminal() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        (void)enableTerminalColors();
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
        std::vector<wchar_t> buffer(MAX_PATH);
        while (buffer.size() <= 32768) {
            DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (len == 0) {
                break;
            }
            if (len < buffer.size()) {
                std::filesystem::path rawPath(buffer.data());
                std::error_code ec;
                auto canonical = std::filesystem::canonical(rawPath, ec);
                if (!ec) {
                    return canonical;
                }
                return rawPath;
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
        return std::filesystem::current_path() / "mge";
    }

} // namespace mge::cli
