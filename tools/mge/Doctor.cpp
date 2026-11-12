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
            item.name = "Compilador C++";
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
                item.detail = "No encontrado";
                item.hint =
                    "Instalar un compilador C++17 (ej: sudo apt install g++ / sudo dnf install gcc-c++)";
                return item;
            }

            std::string output;
            if (runCommandCapture({compiler, "--version"}, output) != 0) {
                item.ok = false;
                item.detail = compiler + " (error al consultar --version)";
                item.hint = "Verificar que el compilador funcione correctamente";
                return item;
            }

            std::string line = firstLine(output);
            // Buscar número de versión con regex
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
                        item.hint = "Se requiere Clang >= 14 para soporte completo de C++17";
                    }
                } else {
                    // Por defecto asumir GCC / compatible
                    if (major >= 11) {
                        item.ok = true;
                    } else {
                        item.ok = false;
                        item.hint = "Se requiere GCC >= 11 para soporte completo de C++17";
                    }
                }
                item.detail = line;
            } else {
                // Si no se puede parsear la versión exacta pero el comando existe
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
                item.detail = "No encontrado";
                item.hint = "Instalar CMake >= 3.24 (https://cmake.org/download/ o gestor de paquetes)";
                return item;
            }

            std::string output;
            if (runCommandCapture({"cmake", "--version"}, output) != 0) {
                item.ok = false;
                item.detail = "Error al ejecutar cmake --version";
                item.hint = "Verificar la instalación de CMake";
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
                    item.detail = line + " (se requiere >= 3.24)";
                    item.hint = "Actualizar CMake a versión >= 3.24";
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
                    item.detail = "versión " + trim(output);
                    return item;
                }
            }

            item.ok = false;
            item.detail = "No encontrado (opcional)";
            item.hint = "Instalar ninja-build para compilaciones más veloces (sudo apt install ninja-build)";
            return item;
        }

        DoctorItem checkSdl3() {
            DoctorItem item;
            item.name = "SDL3";
            item.required = true;

            // 1. Probar pkg-config
            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "sdl3"}, output) == 0 &&
                    !trim(output).empty()) {
                    item.ok = true;
                    item.detail = "versión " + trim(output) + " (" + pkgTool + ")";
                    return item;
                }
            }

            // 2. Probar cabeceras o carpetas CMake estándar
            const std::vector<std::string> searchPaths = {
                "/usr/include/SDL3/SDL.h", "/usr/local/include/SDL3/SDL.h",
                "/usr/lib64/cmake/SDL3",   "/usr/lib/x86_64-linux-gnu/cmake/SDL3",
                "/usr/lib/cmake/SDL3",     "/usr/share/cmake/SDL3"};

            for (const auto& pathStr : searchPaths) {
                if (std::filesystem::exists(pathStr)) {
                    item.ok = true;
                    item.detail = "Instalación del sistema encontrada en " + pathStr;
                    return item;
                }
            }

            item.ok = false;
            item.detail = "No encontrado";
            item.hint =
                "Instalar libsdl3-dev o SDL3-devel (o compilar desde https://github.com/libsdl-org/SDL)";
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
            item.detail = "No encontrado";
            item.hint = "Instalar glslang (sudo apt install glslang-tools / sudo dnf install glslang)";
            return item;
        }

        DoctorItem checkVulkan() {
            DoctorItem item;
            item.name = "Vulkan Loader & ICD";
            item.required = true;

            // Comprobación con vulkaninfo
            if (commandExists("vulkaninfo")) {
                std::string output;
                if (runCommandCapture({"vulkaninfo", "--summary"}, output) == 0) {
                    if (output.find("GPU") != std::string::npos ||
                        output.find("Devices") != std::string::npos ||
                        output.find("Vulkan") != std::string::npos) {
                        item.ok = true;
                        item.detail = "vulkaninfo OK (loader y controlador ICD activos)";
                        return item;
                    }
                }
            }

            // Comprobación alternativa del loader
            bool loaderFound = false;
            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "vulkan"}, output) == 0) {
                    loaderFound = true;
                    item.detail = "Loader Vulkan " + trim(output);
                }
            }

            if (!loaderFound) {
                const std::vector<std::string> loaderPaths = {
                    "/usr/lib64/libvulkan.so.1", "/usr/lib/x86_64-linux-gnu/libvulkan.so.1",
                    "/usr/lib/libvulkan.so.1", "/usr/lib64/libvulkan.so", "/usr/lib/libvulkan.so"};
                for (const auto& pathStr : loaderPaths) {
                    if (std::filesystem::exists(pathStr)) {
                        loaderFound = true;
                        item.detail = "Loader encontrado en " + pathStr;
                        break;
                    }
                }
            }

            // Búsqueda de manifiestos ICD de controladores
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
                item.detail += " (" + std::to_string(icdCount) + " manifiesto(s) ICD detectado(s))";
                return item;
            }

            if (loaderFound) {
                item.ok = false;
                item.detail = "Loader de Vulkan presente pero no se detectaron controladores ICD";
                item.hint = "Instalar controladores Vulkan (mesa-vulkan-drivers o driver NVIDIA/AMD)";
                return item;
            }

            item.ok = false;
            item.detail = "No encontrado";
            item.hint = "Instalar libvulkan-dev y mesa-vulkan-drivers (sudo apt install libvulkan-dev "
                        "mesa-vulkan-drivers)";
            return item;
        }

        DoctorItem checkGlm() {
            DoctorItem item;
            item.name = "Cabeceras GLM";
            item.required = true;

            const std::vector<std::string> glmPaths = {"/usr/include/glm/glm.hpp",
                                                       "/usr/local/include/glm/glm.hpp"};

            for (const auto& pathStr : glmPaths) {
                if (std::filesystem::exists(pathStr)) {
                    item.ok = true;
                    item.detail = "Cabeceras encontradas en " + pathStr;
                    return item;
                }
            }

            std::string pkgTool =
                commandExists("pkg-config") ? "pkg-config" : (commandExists("pkgconf") ? "pkgconf" : "");
            if (!pkgTool.empty()) {
                std::string output;
                if (runCommandCapture({pkgTool, "--modversion", "glm"}, output) == 0) {
                    item.ok = true;
                    item.detail = "GLM versión " + trim(output) + " (" + pkgTool + ")";
                    return item;
                }
            }

            item.ok = false;
            item.detail = "No encontrado";
            item.hint = "Instalar libglm-dev (sudo apt install libglm-dev / sudo dnf install glm-devel)";
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
            item.detail = "No encontrado";
            item.hint = "Instalar Python >= 3.10 (sudo apt install python3)";
            return item;
        }

        std::string output;
        if (runCommandCapture({py, "--version"}, output) != 0) {
            item.ok = false;
            item.detail = "Error al consultar versión de Python";
            item.hint = "Verificar que python3 funcione correctamente";
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
                item.hint = "Se requiere Python >= 3.10";
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
            item.detail = "No encontrado";
            item.hint = "Instalar pip (python3 -m ensurepip o sudo apt install python3-pip)";
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
            item.detail = "Módulo estándar venv disponible";
        } else {
            item.ok = false;
            item.detail = "No encontrado";
            item.hint = "Instalar paquete python3-venv (sudo apt install python3-venv)";
        }
        return item;
    }

    DoctorItem checkPythonMge() {
        DoctorItem item;
        item.name = "mge (bindings de Python)";
        item.required = true;

        std::string py = commandExists("python3") ? "python3" : "python";
        std::string output;
        if (runCommandCapture({py, "-c", "import mge; print(getattr(mge, '__version__', 'instalado'))"},
                              output) == 0) {
            std::string ver = firstLine(output);
            item.ok = true;
            item.detail = "mge v" + ver;
        } else {
            item.ok = false;
            item.detail = "No encontrado en el entorno de Python";
            item.hint = "Instalar el paquete mge (pip install . o pip install mge)";
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
                std::cout << "Uso: mge doctor [--lang <cpp|c|python>]\n\n"
                          << "Comprueba que las herramientas y dependencias del sistema estén instaladas\n"
                          << "y cumplan con los requisitos mínimos de miniGameEngine:\n"
                          << "  - C/C++: Compilador, CMake, Ninja, SDL3, glslang, Vulkan, GLM\n"
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
                  << (isPython ? "mge doctor - Comprobando entorno de desarrollo Python...\n"
                               : "mge doctor - Comprobando dependencias del sistema...\n")
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
                    std::cout << colors::yellow() << "    Sugerencia: " << item.hint << colors::reset()
                              << "\n";
                }
            } else {
                anyRequiredFailed = true;
                std::cout << colors::red() << "[✗] " << colors::reset() << colors::bold() << item.name
                          << colors::reset() << " - " << item.detail << "\n";
                if (!item.hint.empty()) {
                    std::cout << colors::yellow() << "    Sugerencia: " << item.hint << colors::reset()
                              << "\n";
                }
            }
        }

        std::cout << "\n";
        if (anyRequiredFailed) {
            std::cout
                << colors::red() << colors::bold()
                << "Se encontraron problemas en dependencias requeridas. Consulte las sugerencias arriba."
                << colors::reset() << "\n";
            return 1;
        }

        std::cout << colors::green() << colors::bold()
                  << "Todas las dependencias requeridas están correctamente instaladas." << colors::reset()
                  << "\n";
        return 0;
    }

} // namespace mge::cli
