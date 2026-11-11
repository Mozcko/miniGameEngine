#include "Project.h"

#include "Process.h"
#include <mge/Version.h>

#include <cctype>
#include <chrono>
#include <ctime>
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

    std::string renderTemplateString(const std::string& content, const NewProjectOptions& options,
                                     const std::string& className, const std::string& title,
                                     const std::string& version) {
        std::string result = content;
        replaceAll(result, "@GAME_NAME@", options.name);
        replaceAll(result, "@GAME_CLASS@", className);
        replaceAll(result, "@GAME_TITLE@", title);
        replaceAll(result, "@MGE_VERSION@", version);

        if (options.templateName == "2d") {
            if (options.audio) {
                replaceAll(result, "@GAME_MODULES@", "platform gpu graphics audio");
                replaceAll(result, "@AUDIO_ADD_MODULE@", "addModule<mge::AudioModule>();");
                replaceAll(result, "@AUDIO_INIT_MODULE@",
                           "m_audio = getModule<mge::AudioModule>();\n"
                           "            if (!m_platform || !m_gpu || !m_audio) return false;");
                replaceAll(result, "@AUDIO_LOAD_SFX@",
                           "// 3. Audio procedural sintetizado\n"
                           "            m_coinSfx = m_audio->addSfx(mge::sfx::coin());");
                replaceAll(result, "@AUDIO_PLAY_SFX@",
                           "if (collected > 0) {\n"
                           "                m_audio->play(m_coinSfx);\n"
                           "            }");
                replaceAll(result, "@AUDIO_MEMBER@",
                           "mge::AudioModule* m_audio = nullptr;\n"
                           "        mge::SampleId m_coinSfx{};");
            } else {
                replaceAll(result, "@GAME_MODULES@", "platform gpu graphics");
                replaceAll(result, "@AUDIO_ADD_MODULE@", "// Módulo de audio desactivado");
                replaceAll(result, "@AUDIO_INIT_MODULE@", "if (!m_platform || !m_gpu) return false;");
                replaceAll(result, "@AUDIO_LOAD_SFX@", "// Audio desactivado");
                replaceAll(result, "@AUDIO_PLAY_SFX@", "// Audio desactivado");
                replaceAll(result, "@AUDIO_MEMBER@", "// Audio desactivado");
            }
        } else if (options.templateName == "3d") {
            if (options.audio) {
                replaceAll(result, "@GAME_MODULES@", "platform gpu graphics audio physics procgen");
                replaceAll(result, "@AUDIO_ADD_MODULE@", "addModule<mge::AudioModule>();");
                replaceAll(result, "@AUDIO_INIT_MODULE@",
                           "m_audio = getModule<mge::AudioModule>();\n"
                           "            if (!m_platform || !m_gpu || !m_audio) return false;");
                replaceAll(result, "@AUDIO_LOAD_SFX@",
                           "// 4. Audio sintetizado procedural\n"
                           "            m_jumpSfx = m_audio->addSfx(mge::sfx::jump());");
                replaceAll(result, "@AUDIO_PLAY_SFX@",
                           "const bool wantJump = m_platform->wasActionPressed(\"Jump\");\n"
                           "            if (wantJump && controller.grounded) {\n"
                           "                m_audio->play(m_jumpSfx);\n"
                           "            }");
                replaceAll(result, "@AUDIO_MEMBER@",
                           "mge::AudioModule* m_audio = nullptr;\n"
                           "        mge::SampleId m_jumpSfx{};");
            } else {
                replaceAll(result, "@GAME_MODULES@", "platform gpu graphics physics procgen");
                replaceAll(result, "@AUDIO_ADD_MODULE@", "// Módulo de audio desactivado");
                replaceAll(result, "@AUDIO_INIT_MODULE@", "if (!m_platform || !m_gpu) return false;");
                replaceAll(result, "@AUDIO_LOAD_SFX@", "// Audio desactivado");
                replaceAll(result, "@AUDIO_PLAY_SFX@",
                           "const bool wantJump = m_platform->wasActionPressed(\"Jump\");\n"
                           "            (void)wantJump;");
                replaceAll(result, "@AUDIO_MEMBER@", "// Audio desactivado");
            }
        }

        return result;
    }

    std::string renderTemplateString(const std::string& content, const std::string& name,
                                     const std::string& className, const std::string& title,
                                     const std::string& version) {
        NewProjectOptions opt;
        opt.name = name;
        opt.templateName = "3d";
        opt.audio = true;
        return renderTemplateString(content, opt, className, title, version);
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
                std::string rendered = renderTemplateString(buffer.str(), options, className, title, version);

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

        // Crear archivo de licencia LICENSE si no es None
        if (options.license != LicenseType::None) {
            std::string author;
            runCommandCapture({"git", "config", "user.name"}, author);
            while (!author.empty() &&
                   (author.back() == '\n' || author.back() == '\r' || author.back() == ' ')) {
                author.pop_back();
            }
            if (author.empty()) {
                const char* userEnv = std::getenv("USER");
                if (userEnv == nullptr) {
                    userEnv = std::getenv("USERNAME");
                }
                author = (userEnv != nullptr) ? userEnv : "MiniGameEngine Contributor";
            }

            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tmTime{};
#ifdef _WIN32
            gmtime_s(&tmTime, &tt);
#else
            gmtime_r(&tt, &tmTime);
#endif
            std::string yearStr = std::to_string(1900 + tmTime.tm_year);

            std::ofstream licFile(dest / "LICENSE");
            if (options.license == LicenseType::Mit) {
                licFile << "MIT License\n\n"
                        << "Copyright (c) " << yearStr << " " << author << "\n\n"
                        << "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
                        << "of this software and associated documentation files (the \"Software\"), to deal\n"
                        << "in the Software without restriction, including without limitation the rights\n"
                        << "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
                        << "copies of the Software, and to permit persons to whom the Software is\n"
                        << "furnished to do so, subject to the following conditions:\n\n"
                        << "The above copyright notice and this permission notice shall be included in all\n"
                        << "copies or substantial portions of the Software.\n\n"
                        << "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
                        << "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
                        << "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
                        << "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
                        << "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
                        << "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
                        << "SOFTWARE.\n";
            } else if (options.license == LicenseType::Apache2) {
                licFile << "                                 Apache License\n"
                        << "                           Version 2.0, January 2004\n"
                        << "                        http://www.apache.org/licenses/\n\n"
                        << "   Copyright " << yearStr << " " << author << "\n\n"
                        << "   Licensed under the Apache License, Version 2.0 (the \"License\");\n"
                        << "   you may not use this file except in compliance with the License.\n"
                        << "   You may obtain a copy of the License at\n\n"
                        << "       http://www.apache.org/licenses/LICENSE-2.0\n\n"
                        << "   Unless required by applicable law or agreed to in writing, software\n"
                        << "   distributed under the License is distributed on an \"AS IS\" BASIS,\n"
                        << "   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
                        << "   See the License for the specific language governing permissions and\n"
                        << "   limitations under the License.\n";
            }
        }

        // Crear configuración de VS Code (.vscode/) si está activada
        if (options.vscode) {
            std::filesystem::path vscodeDir = dest / ".vscode";
            std::filesystem::create_directories(vscodeDir, ec);

            std::ofstream tasksFile(vscodeDir / "tasks.json");
            tasksFile
                << "{\n"
                << "    \"version\": \"2.0.0\",\n"
                << "    \"tasks\": [\n"
                << "        {\n"
                << "            \"label\": \"Configure (dev)\",\n"
                << "            \"type\": \"shell\",\n"
                << "            \"command\": \"cmake --preset dev\",\n"
                << "            \"problemMatcher\": []\n"
                << "        },\n"
                << "        {\n"
                << "            \"label\": \"Build (dev)\",\n"
                << "            \"type\": \"shell\",\n"
                << "            \"command\": \"test -f build/CMakeCache.txt || cmake --preset dev; cmake "
                   "--build --preset dev\",\n"
                << "            \"group\": { \"kind\": \"build\", \"isDefault\": true },\n"
                << "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
                << "        },\n"
                << "        {\n"
                << "            \"label\": \"Run game\",\n"
                << "            \"type\": \"shell\",\n"
                << "            \"command\": \"mge run\",\n"
                << "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
                << "        },\n"
                << "        {\n"
                << "            \"label\": \"Test (dev)\",\n"
                << "            \"type\": \"shell\",\n"
                << "            \"command\": \"mge test\",\n"
                << "            \"group\": { \"kind\": \"test\", \"isDefault\": true },\n"
                << "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
                << "        },\n"
                << "        {\n"
                << "            \"label\": \"Check (format)\",\n"
                << "            \"type\": \"shell\",\n"
                << "            \"command\": \"mge check\",\n"
                << "            \"problemMatcher\": []\n"
                << "        }\n"
                << "    ]\n"
                << "}\n";

            std::ofstream settingsFile(vscodeDir / "settings.json");
            settingsFile << "{\n"
                         << "    \"cmake.configureOnOpen\": false\n"
                         << "}\n";
        }

        // Crear archivo de metadata mge.project
        {
            std::string licName = (options.license == LicenseType::Mit)
                                      ? "mit"
                                      : ((options.license == LicenseType::Apache2) ? "apache" : "none");
            std::ofstream projFile(dest / "mge.project");
            projFile << "name=" << options.name << "\n";
            projFile << "template=" << tmplName << "\n";
            projFile << "audio=" << (options.audio ? "yes" : "no") << "\n";
            projFile << "license=" << licName << "\n";
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

        // Inicializar repositorio Git salvo que se indique --no-git o git=false
        bool wantGit = options.git && !options.noGit;
        if (wantGit && commandExists("git")) {
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
