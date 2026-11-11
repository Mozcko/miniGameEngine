#include "Project.h"

#include "Process.h"
#include <mge/Version.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace mge::cli {

    namespace {

        void replaceAll(std::string& str, const std::string& from, const std::string& to) {
            if (from.empty()) {
                return;
            }
            size_t pos = 0;
            while ((pos = str.find(from, pos)) != std::string::npos) {
                str.replace(pos, from.length(), to);
                pos += to.length();
            }
        }

        const std::vector<std::string> kReservedNames = {
            "sandbox",     "run",       "mge_core",    "mge_main",  "mge_platform", "mge_gpu", "mge_graphics",
            "mge_procgen", "mge_voxel", "mge_physics", "mge_synth", "mge_audio",    "build",   "templates",
            "tests",       "docs",      "scripts",     "tools",     "cmake",        "examples"};

    } // namespace

    bool validateProjectName(const std::string& name, std::string& outError) {
        outError.clear();
        if (name.empty()) {
            outError = "El nombre del proyecto no puede estar vacío.";
            return false;
        }

        // Primer carácter: letra minúscula a-z
        char first = name.front();
        if (first < 'a' || first > 'z') {
            outError = "Nombre inválido '" + name + "': debe comenzar con una letra minúscula (a-z).";
            return false;
        }

        // Resto de caracteres: a-z, 0-9, o _
        for (char ch : name) {
            bool valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || (ch == '_');
            if (!valid) {
                outError =
                    "Nombre inválido '" + name + "': solo se admiten minúsculas, números y guión bajo (_).";
                return false;
            }
        }

        // Comprobación de palabras reservadas
        for (const auto& reserved : kReservedNames) {
            if (name == reserved) {
                outError = "El nombre '" + name + "' está reservado por el motor.";
                return false;
            }
        }

        return true;
    }

    std::string toPascalCase(const std::string& snakeCase) {
        std::string result;
        bool capitalizeNext = true;
        for (char ch : snakeCase) {
            if (ch == '_') {
                capitalizeNext = true;
            } else {
                if (capitalizeNext) {
                    result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    capitalizeNext = false;
                } else {
                    result += ch;
                }
            }
        }
        return result;
    }

    std::string toTitleCase(const std::string& snakeCase) {
        std::string result;
        bool capitalizeNext = true;
        for (char ch : snakeCase) {
            if (ch == '_') {
                if (!result.empty()) {
                    result += ' ';
                }
                capitalizeNext = true;
            } else {
                if (capitalizeNext) {
                    result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    capitalizeNext = false;
                } else {
                    result += ch;
                }
            }
        }
        return result;
    }

    std::string renderTemplateString(const std::string& content, const std::string& name,
                                     const std::string& className, const std::string& title,
                                     const std::string& version) {
        std::string result = content;
        replaceAll(result, "@GAME_NAME@", name);
        replaceAll(result, "@GAME_CLASS@", className);
        replaceAll(result, "@GAME_TITLE@", title);
        replaceAll(result, "@MGE_VERSION@", version);
        return result;
    }

    std::filesystem::path findTemplatesDir(const std::filesystem::path& exePath) {
        // 1. Variable de entorno MGE_TEMPLATES_DIR
        const char* envTmpl = std::getenv("MGE_TEMPLATES_DIR");
        if (envTmpl != nullptr && *envTmpl != '\0') {
            std::filesystem::path envPath(envTmpl);
            if (std::filesystem::is_directory(envPath)) {
                return std::filesystem::canonical(envPath);
            }
        }

        // 2. Ruta instalada relativa al binario (<exe_dir>/../share/mge/templates)
        if (!exePath.empty()) {
            std::filesystem::path sharePath = exePath.parent_path() / ".." / "share" / "mge" / "templates";
            if (std::filesystem::is_directory(sharePath)) {
                return std::filesystem::canonical(sharePath);
            }
        }

        // 3. Directorio del árbol de código fuente (compilaciones de desarrollo)
#ifdef MGE_SOURCE_TEMPLATES_DIR
        std::filesystem::path srcTmplPath(MGE_SOURCE_TEMPLATES_DIR);
        if (std::filesystem::is_directory(srcTmplPath)) {
            return std::filesystem::canonical(srcTmplPath);
        }
#endif

        // 4. Búsqueda relativa en el directorio de trabajo actual
        if (std::filesystem::is_directory("templates")) {
            return std::filesystem::canonical("templates");
        }
        if (std::filesystem::is_directory("../templates")) {
            return std::filesystem::canonical("../templates");
        }

        return {};
    }

    ProjectInfo detectProject(const std::filesystem::path& startDir) {
        std::filesystem::path current = std::filesystem::canonical(startDir);

        while (true) {
            // Comprobar archivo mge.project
            std::filesystem::path projFile = current / "mge.project";
            if (std::filesystem::is_regular_file(projFile)) {
                std::ifstream in(projFile);
                std::string line;
                std::string name;
                std::string templateType;
                while (std::getline(in, line)) {
                    if (line.rfind("name=", 0) == 0) {
                        name = line.substr(5);
                    } else if (line.rfind("template=", 0) == 0) {
                        templateType = line.substr(9);
                    }
                }
                return ProjectInfo{ProjectType::GameProject, name, templateType, current};
            }

            // Comprobar CMakeLists.txt del repositorio del motor
            std::filesystem::path cmakeFile = current / "CMakeLists.txt";
            if (std::filesystem::is_regular_file(cmakeFile)) {
                std::ifstream in(cmakeFile);
                std::stringstream buffer;
                buffer << in.rdbuf();
                std::string content = buffer.str();

                if (content.find("project(miniGameEngine") != std::string::npos) {
                    return ProjectInfo{ProjectType::EngineRepo, "miniGameEngine", "", current};
                }

                if (content.find("mge_add_game(") != std::string::npos) {
                    // Extraer nombre del juego del comando mge_add_game(nombre ...)
                    size_t pos = content.find("mge_add_game(");
                    size_t start = pos + 13;
                    while (start < content.size() &&
                           std::isspace(static_cast<unsigned char>(content[start]))) {
                        ++start;
                    }
                    size_t end = start;
                    while (end < content.size() && !std::isspace(static_cast<unsigned char>(content[end])) &&
                           content[end] != ')' && content[end] != '\n' && content[end] != '\r') {
                        ++end;
                    }
                    std::string gameName = content.substr(start, end - start);
                    return ProjectInfo{ProjectType::GameProject, gameName, "", current};
                }
            }

            std::filesystem::path parent = current.parent_path();
            if (parent == current || parent.empty()) {
                break;
            }
            current = parent;
        }

        return ProjectInfo{ProjectType::None, "", "", {}};
    }

    int createNewProject(const NewProjectOptions& options, const std::filesystem::path& exePath,
                         std::string& outMessage) {
        std::string error;
        if (!validateProjectName(options.name, error)) {
            outMessage = error;
            return 1;
        }

        std::filesystem::path tmplBase = findTemplatesDir(exePath);
        if (tmplBase.empty() || !std::filesystem::is_directory(tmplBase)) {
            outMessage = "Error: no se pudo localizar la carpeta de plantillas.";
            return 1;
        }

        std::string tmplName = options.templateName.empty() ? "3d" : options.templateName;
        std::filesystem::path tmplDir = tmplBase / tmplName;
        if (!std::filesystem::is_directory(tmplDir)) {
            outMessage = "Error: no existe la plantilla '" + tmplName + "' en " + tmplBase.string();
            return 1;
        }

        std::filesystem::path dest = options.targetDir;
        if (dest.empty()) {
            dest = std::filesystem::current_path() / options.name;
        }

        if (std::filesystem::exists(dest)) {
            outMessage = "Error: la ruta de destino ya existe: " + dest.string();
            return 1;
        }

        std::error_code ec;
        if (!std::filesystem::create_directories(dest, ec)) {
            outMessage = "Error al crear la carpeta destino '" + dest.string() + "': " + ec.message();
            return 1;
        }

        std::string className = toPascalCase(options.name);
        std::string title = toTitleCase(options.name);
        std::string version = MGE_VERSION_STRING;

        // Recorrer y copiar archivos de la plantilla recursivamente
        for (const auto& entry : std::filesystem::recursive_directory_iterator(tmplDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::filesystem::path rel = std::filesystem::relative(entry.path(), tmplDir);
            std::string relStr = rel.string();

            // Reemplazo de comodines en rutas
            replaceAll(relStr, "@GAME_CLASS@", className);
            replaceAll(relStr, "@GAME_NAME@", options.name);
            if (rel.filename().string() == "Game.cpp.in" || rel.filename().string() == "Game.cpp") {
                replaceAll(relStr, "Game.cpp", className + ".cpp");
            }

            bool isTemplateIn = false;
            const std::string inSuffix = ".in";
            if (relStr.size() >= inSuffix.size() &&
                relStr.compare(relStr.size() - inSuffix.size(), inSuffix.size(), inSuffix) == 0) {
                isTemplateIn = true;
                relStr = relStr.substr(0, relStr.size() - inSuffix.size());
            }

            std::filesystem::path targetFile = dest / relStr;
            std::filesystem::create_directories(targetFile.parent_path(), ec);

            if (isTemplateIn) {
                std::ifstream src(entry.path());
                std::stringstream buffer;
                buffer << src.rdbuf();
                std::string rendered =
                    renderTemplateString(buffer.str(), options.name, className, title, version);

                std::ofstream dst(targetFile, std::ios::trunc);
                dst << rendered;
            } else {
                std::filesystem::copy_file(entry.path(), targetFile,
                                           std::filesystem::copy_options::overwrite_existing, ec);
            }

            // Formatear archivos C++ generados si clang-format está disponible
            std::string ext = targetFile.extension().string();
            if ((ext == ".cpp" || ext == ".h") && commandExists("clang-format")) {
                runCommand({"clang-format", "-i", targetFile.string()});
            }
        }

        // Crear archivo de metadata mge.project
        {
            std::ofstream projFile(dest / "mge.project");
            projFile << "name=" << options.name << "\n";
            projFile << "template=" << tmplName << "\n";
        }

        // Si se crea en una subcarpeta de un repo CMake (ej: examples/), registrar en el CMakeLists padre
        std::filesystem::path parentCmake = dest.parent_path() / "CMakeLists.txt";
        std::string registeredMsg;
        if (std::filesystem::is_regular_file(parentCmake)) {
            std::ifstream in(parentCmake);
            std::stringstream buffer;
            buffer << in.rdbuf();
            std::string content = buffer.str();
            std::string subCmd = "add_subdirectory(" + options.name + ")";
            if (content.find(subCmd) == std::string::npos) {
                std::ofstream out(parentCmake, std::ios::app);
                out << subCmd << "\n";
            }
            registeredMsg = " Registrado en " + parentCmake.string() + ".";
        }

        // Inicializar repositorio Git salvo que se indique --no-git
        if (!options.noGit && commandExists("git")) {
            runCommand({"git", "init", "-q"}, dest);
        }

        std::stringstream out;
        out << "Creado '" << dest.string() << "' (plantilla " << tmplName << ", clase " << className << ")."
            << registeredMsg << "\n\n"
            << "Siguientes pasos:\n"
            << "  cd " << dest.string() << "\n"
            << "  mge run\n";

        outMessage = out.str();
        return 0;
    }

} // namespace mge::cli
