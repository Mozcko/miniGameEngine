#include "Doctor.h"

#include "Colors.h"
#include "Process.h"
#include "Project.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <string_view>

namespace mge::cli {

    namespace {

        std::string trim(const std::string& str) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                ++start;
            }
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
                --end;
            }
            return str.substr(start, end - start);
        }

        std::string firstLine(const std::string& str) {
            std::stringstream ss(str);
            std::string line;
            if (std::getline(ss, line)) {
                return trim(line);
            }
            return trim(str);
        }

        DoctorItem checkCompiler() {
            DoctorItem item;
            item.name = "C++ Compiler";
            item.required = true;

            std::string compiler;
            const char* cxxEnv = std::getenv("CXX");
            if (cxxEnv != nullptr && commandExists(cxxEnv)) {
                compiler = cxxEnv;
            } else if (commandExists("g++")) {
                compiler = "g++";
            } else if (commandExists("clang++")) {
                compiler = "clang++";
            } else if (commandExists("c++")) {
                compiler = "c++";
            }

            if (compiler.empty()) {
                item.ok = false;
                item.detail = "Not found";
                item.hint = "Install a C++17 compiler (e.g., sudo apt install g++ / sudo dnf install gcc-c++)";
                return item;
            }

            std::string output;
            if (runCommandCapture({compiler, "--version"}, output) != 0) {
                item.ok = false;
                item.detail = compiler + " (error querying --version)";
                item.hint = "Verify that the compiler works properly";
                return item;
            }

            std::string line = firstLine(output);
            // Search version number with regex
            std::regex verRegex(R"((\d+)\.(\d+)(\.(\d+))?)");
            std::smatch match;
            if (std::regex_search(line, match, verRegex)) {
                int major = std::stoi(match[1].str());
                bool isClang =
                    line.find("clang") != std::string::npos || line.find("Clang") != std::string::npos;
                if (isClang) {
                    if (major >= 14) {
                        item.ok = true;
                    } else {
                        item.ok = false;
                        item.hint = "Clang >= 14 is required for full C++17 support";
                    }
                } else {
                    // Default to assuming GCC / compatible
                    if (major >= 11) {
                        item.ok = true;
                    } else {
                        item.ok = false;
                        item.hint = "GCC >= 11 is required for full C++17 support";
                    }
                }
                item.detail = line;
            } else {
                // If exact version cannot be parsed but command exists
                item.ok = true;
                item.detail = line;
            }

            return item;
        }

        DoctorItem checkCmake() {
            DoctorItem item;
            item.name = "CMake";
            item.required = true;

            if (!commandExists("cmake")) {
                item.ok = false;
                item.detail = "Not found";
                item.hint = "Install CMake >= 3.24 (https://cmake.org/download/ or package manager)";
                return item;
            }

            std::string output;
            if (runCommandCapture({"cmake", "--version"}, output) != 0) {
                item.ok = false;
                item.detail = "Error running cmake --version";
                item.hint = "Verify your CMake installation";
                return item;
            }

            std::string line = firstLine(output);
            std::regex verRegex(R"(version\s+(\d+)\.(\d+)(\.(\d+))?)");
            std::smatch match;
            if (std::regex_search(line, match, verRegex)) {
                int major = std::stoi(match[1].str());
                int minor = std::stoi(match[2].str());
                if (major > 3 || (major == 3 && minor >= 24)) {
                    item.ok = true;
                    item.detail = line;
                } else {
                    item.ok = false;
                    item.detail = line + " (requires >= 3.24)";
                    item.hint = "Update CMake to version >= 3.24";
                }
            } else {
                item.ok = true;
                item.detail = line;
            }

            return item;
        }

        DoctorItem checkNinja() {
            DoctorItem item;
            item.name = "Ninja";
            item.required = false;

            if (commandExists("ninja")) {
                std::string output;
                if (runCommandCapture({"ninja", "--version"}, output) == 0) {
                    item.ok = true;
                    item.detail = "version " + trim(output);
                    return item;
                }
            }

            item.ok = false;
            item.detail = "Not found (optional)";
            item.hint = "Install ninja-build for faster builds (sudo apt install ninja-build)";
            return item;
        }

        DoctorItem checkSdl3() {
            DoctorItem item;
            item.name = "SDL3";
            item.required = true;

            // 1. Try pkg-config
            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "sdl3"}, output) == 0 &&
                    !trim(output).empty()) {
                    item.ok = true;
                    item.detail = "version " + trim(output) + " (" + pkgTool + ")";
                    return item;
                }
            }

            // 2. Try standard headers or CMake directories
            const std::vector<std::string> searchPaths = {
                "/usr/include/SDL3/SDL.h", "/usr/local/include/SDL3/SDL.h",
                "/usr/lib64/cmake/SDL3",   "/usr/lib/x86_64-linux-gnu/cmake/SDL3",
                "/usr/lib/cmake/SDL3",     "/usr/share/cmake/SDL3"};

            for (const auto& pathStr : searchPaths) {
                if (std::filesystem::exists(pathStr)) {
                    item.ok = true;
                    item.detail = "System installation found at " + pathStr;
                    return item;
                }
            }

            item.ok = false;
            item.detail = "Not found";
            item.hint =
                "Install libsdl3-dev or SDL3-devel (or build from https://github.com/libsdl-org/SDL)";
            return item;
        }

        DoctorItem checkGlslang() {
            DoctorItem item;
            item.name = "glslang";
            item.required = true;

            if (commandExists("glslang")) {
                std::string output;
                if (runCommandCapture({"glslang", "--version"}, output) == 0) {
                    item.ok = true;
                    item.detail = firstLine(output);
                    return item;
                }
            }

            if (commandExists("glslangValidator")) {
                std::string output;
                if (runCommandCapture({"glslangValidator", "-v"}, output) == 0) {
                    item.ok = true;
                    item.detail = firstLine(output);
                    return item;
                }
            }

            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install glslang (sudo apt install glslang-tools / sudo dnf install glslang)";
            return item;
        }

        DoctorItem checkVulkan() {
            DoctorItem item;
            item.name = "Vulkan Loader & ICD";
            item.required = true;

            // Check with vulkaninfo
            if (commandExists("vulkaninfo")) {
                std::string output;
                if (runCommandCapture({"vulkaninfo", "--summary"}, output) == 0) {
                    if (output.find("GPU") != std::string::npos ||
                        output.find("Devices") != std::string::npos ||
                        output.find("Vulkan") != std::string::npos) {
                        item.ok = true;
                        item.detail = "vulkaninfo OK (loader and ICD driver active)";
                        return item;
                    }
                }
            }

            // Alternative loader check
            bool loaderFound = false;
            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "vulkan"}, output) == 0) {
                    loaderFound = true;
                    item.detail = "Vulkan Loader " + trim(output);
                }
            }

            if (!loaderFound) {
                const std::vector<std::string> loaderPaths = {
                    "/usr/lib64/libvulkan.so.1", "/usr/lib/x86_64-linux-gnu/libvulkan.so.1",
                    "/usr/lib/libvulkan.so.1", "/usr/lib64/libvulkan.so", "/usr/lib/libvulkan.so"};
                for (const auto& pathStr : loaderPaths) {
                    if (std::filesystem::exists(pathStr)) {
                        loaderFound = true;
                        item.detail = "Loader found at " + pathStr;
                        break;
                    }
                }
            }

            // Search for driver ICD manifests
            size_t icdCount = 0;
            const std::vector<std::string> icdDirs = {"/usr/share/vulkan/icd.d", "/etc/vulkan/icd.d"};

            for (const auto& dirStr : icdDirs) {
                if (std::filesystem::is_directory(dirStr)) {
                    for (const auto& entry : std::filesystem::directory_iterator(dirStr)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".json") {
                            ++icdCount;
                        }
                    }
                }
            }

            const char* driverFiles = std::getenv("VK_DRIVER_FILES");
            if (driverFiles != nullptr && std::filesystem::exists(driverFiles)) {
                ++icdCount;
            }
            const char* icdFilenames = std::getenv("VK_ICD_FILENAMES");
            if (icdFilenames != nullptr && std::filesystem::exists(icdFilenames)) {
                ++icdCount;
            }

            if (loaderFound && icdCount > 0) {
                item.ok = true;
                item.detail += " (" + std::to_string(icdCount) + " ICD manifest(s) detected)";
                return item;
            }

            if (loaderFound) {
                item.ok = false;
                item.detail = "Vulkan loader present but no ICD drivers detected";
                item.hint = "Install Vulkan drivers (mesa-vulkan-drivers or NVIDIA/AMD driver)";
                return item;
            }

            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install libvulkan-dev and mesa-vulkan-drivers (sudo apt install libvulkan-dev "
                        "mesa-vulkan-drivers)";
            return item;
        }

        DoctorItem checkGlm() {
            DoctorItem item;
            item.name = "GLM Headers";
            item.required = true;

            const std::vector<std::string> glmPaths = {"/usr/include/glm/glm.hpp",
                                                       "/usr/local/include/glm/glm.hpp"};

            for (const auto& pathStr : glmPaths) {
                if (std::filesystem::exists(pathStr)) {
                    item.ok = true;
                    item.detail = "Headers found at " + pathStr;
                    return item;
                }
            }

            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "glm"}, output) == 0) {
                    item.ok = true;
                    item.detail = "GLM version " + trim(output) + " (" + pkgTool + ")";
                    return item;
                }
            }

            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install libglm-dev (sudo apt install libglm-dev / sudo dnf install glm-devel)";
            return item;
        }

    } // namespace

    std::vector<DoctorItem> runDoctorChecks() {
        std::vector<DoctorItem> items;
        items.push_back(checkCompiler());
        items.push_back(checkCmake());
        items.push_back(checkNinja());
        items.push_back(checkSdl3());
        items.push_back(checkGlslang());
        items.push_back(checkVulkan());
        items.push_back(checkGlm());
        return items;
    }

    DoctorItem checkPythonVersion() {
        DoctorItem item;
        item.name = "Python 3";
        item.required = true;

        std::string py = commandExists("python3") ? "python3" : (commandExists("python") ? "python" : "");
        if (py.empty()) {
            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install Python >= 3.10 (sudo apt install python3)";
            return item;
        }

        std::string output;
        if (runCommandCapture({py, "--version"}, output) != 0) {
            item.ok = false;
            item.detail = "Error querying Python version";
            item.hint = "Verify that python3 works properly";
            return item;
        }

        std::string line = firstLine(output);
        std::regex verRegex(R"(Python\s+(\d+)\.(\d+)(\.(\d+))?)");
        std::smatch match;
        if (std::regex_search(line, match, verRegex)) {
            int major = std::stoi(match[1].str());
            int minor = std::stoi(match[2].str());
            if (major == 3 && minor >= 10) {
                item.ok = true;
            } else {
                item.ok = false;
                item.hint = "Python >= 3.10 is required";
            }
            item.detail = line;
        } else {
            item.ok = true;
            item.detail = line;
        }
        return item;
    }

    DoctorItem checkPythonPip() {
        DoctorItem item;
        item.name = "pip";
        item.required = true;

        std::string py = commandExists("python3") ? "python3" : "python";
        std::string output;
        if (runCommandCapture({py, "-m", "pip", "--version"}, output) == 0) {
            item.ok = true;
            item.detail = firstLine(output);
        } else {
            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install pip (python3 -m ensurepip or sudo apt install python3-pip)";
        }
        return item;
    }

    DoctorItem checkPythonVenv() {
        DoctorItem item;
        item.name = "venv";
        item.required = true;

        std::string py = commandExists("python3") ? "python3" : "python";
        std::string output;
        if (runCommandCapture({py, "-c", "import venv; print('OK')"}, output) == 0 &&
            output.find("OK") != std::string::npos) {
            item.ok = true;
            item.detail = "Standard venv module available";
        } else {
            item.ok = false;
            item.detail = "Not found";
            item.hint = "Install python3-venv package (sudo apt install python3-venv)";
        }
        return item;
    }

    DoctorItem checkPythonMge() {
        DoctorItem item;
        item.name = "mge (Python bindings)";
        item.required = true;

        std::string py = commandExists("python3") ? "python3" : "python";
        std::string output;
        if (runCommandCapture({py, "-c", "import mge; print(getattr(mge, '__version__', 'installed'))"},
                              output) == 0) {
            std::string ver = firstLine(output);
            item.ok = true;
            item.detail = "mge v" + ver;
        } else {
            item.ok = false;
            item.detail = "Not found in Python environment";
            item.hint = "Install the mge package (pip install . or pip install mge)";
        }
        return item;
    }

    std::vector<DoctorItem> runPythonDoctorChecks() {
        std::vector<DoctorItem> items;
        items.push_back(checkPythonVersion());
        items.push_back(checkPythonPip());
        items.push_back(checkPythonVenv());
        items.push_back(checkPythonMge());
        return items;
    }

    int cmdDoctor(int argc, char* argv[]) {
        bool isPython = false;
        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--lang" && i + 1 < argc) {
                std::string l = argv[++i];
                if (l == "python" || l == "py") {
                    isPython = true;
                }
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: mge doctor [--lang <cpp|c|python>]\n\n"
                          << "Checks that system tools and dependencies are installed\n"
                          << "and meet miniGameEngine minimum requirements:\n"
                          << "  - C/C++: Compiler, CMake, Ninja, SDL3, glslang, Vulkan, GLM\n"
                          << "  - Python: python3 >= 3.10, pip, venv, import mge\n";
                return 0;
            }
        }

        if (!isPython) {
            ProjectInfo proj = detectProject(std::filesystem::current_path());
            if (proj.language == ProjectLanguage::Python) {
                isPython = true;
            }
        }

        std::cout << colors::bold()
                  << (isPython ? "mge doctor - Checking Python development environment...\n"
                               : "mge doctor - Checking system dependencies...\n")
                  << colors::reset() << "\n";

        auto items = isPython ? runPythonDoctorChecks() : runDoctorChecks();
        bool anyRequiredFailed = false;

        for (const auto& item : items) {
            if (item.ok) {
                std::cout << colors::green() << "[✓] " << colors::reset() << colors::bold() << item.name
                          << colors::reset() << " - " << item.detail << "\n";
            } else if (!item.required) {
                std::cout << colors::yellow() << "[!] " << colors::reset() << colors::bold() << item.name
                          << colors::reset() << " - " << item.detail << "\n";
                if (!item.hint.empty()) {
                    std::cout << colors::yellow() << "    Hint: " << item.hint << colors::reset() << "\n";
                }
            } else {
                anyRequiredFailed = true;
                std::cout << colors::red() << "[✗] " << colors::reset() << colors::bold() << item.name
                          << colors::reset() << " - " << item.detail << "\n";
                if (!item.hint.empty()) {
                    std::cout << colors::yellow() << "    Hint: " << item.hint << colors::reset() << "\n";
                }
            }
        }

        std::cout << "\n";
        if (anyRequiredFailed) {
            std::cout
                << colors::red() << colors::bold()
                << "Issues were found with required dependencies. See the hints above."
                << colors::reset() << "\n";
            return 1;
        }

        std::cout << colors::green() << colors::bold()
                  << "All required dependencies are properly installed." << colors::reset() << "\n";
        return 0;
    }

} // namespace mge::cli
