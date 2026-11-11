#include "Commands.h"

#include "Colors.h"
#include "Process.h"
#include "Project.h"
#include <mge/Version.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace mge::cli {

    namespace {

        std::filesystem::path findExecutableBinary(const std::filesystem::path& rootDir,
                                                   const std::string& targetName, const std::string& preset) {
            std::vector<std::filesystem::path> candidates = {
                rootDir / "build" / targetName,
                rootDir / "build" / (targetName + ".exe"),
                rootDir / ("build-" + preset) / targetName,
                rootDir / ("build-" + preset) / (targetName + ".exe"),
            };

            for (const auto& path : candidates) {
                if (std::filesystem::is_regular_file(path)) {
                    return path;
                }
            }

            // Búsqueda recursiva dentro de carpetas de compilación
            std::vector<std::filesystem::path> buildDirs = {rootDir / "build", rootDir / ("build-" + preset)};

            for (const auto& bDir : buildDirs) {
                if (std::filesystem::is_directory(bDir)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(bDir)) {
                        if (entry.is_regular_file()) {
                            std::string stem = entry.path().stem().string();
                            std::string filename = entry.path().filename().string();
                            if (stem == targetName || filename == targetName) {
                                return entry.path();
                            }
                        }
                    }
                }
            }

            return {};
        }

    } // namespace

    int cmdNew(int argc, char* argv[], const std::filesystem::path& exePath) {
        NewProjectOptions options;

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Uso: mge new <nombre> [opciones]\n\n"
                    << "Crea un nuevo proyecto a partir de una plantilla estándar.\n\n"
                    << "Opciones:\n"
                    << "  --template <3d|2d|headless>  Plantilla a emplear (por defecto: 3d)\n"
                    << "  --dir <ruta>                 Carpeta de destino donde inicializar el proyecto\n"
                    << "  --no-git                     No inicializar un repositorio Git automáticamente\n"
                    << "  -h, --help                   Muestra esta ayuda\n";
                return 0;
            }
            if (arg == "--template") {
                if (i + 1 < argc) {
                    options.templateName = argv[++i];
                } else {
                    std::cerr << "Error: --template requiere un argumento (3d, 2d, headless).\n";
                    return 1;
                }
            } else if (arg == "--dir") {
                if (i + 1 < argc) {
                    options.targetDir = argv[++i];
                } else {
                    std::cerr << "Error: --dir requiere una ruta como argumento.\n";
                    return 1;
                }
            } else if (arg == "--no-git") {
                options.noGit = true;
            } else if (arg.rfind("-", 0) == 0) {
                std::cerr << "Error: opción desconocida '" << arg
                          << "'. Ejecute 'mge new --help' para ayuda.\n";
                return 1;
            } else {
                if (options.name.empty()) {
                    options.name = std::string(arg);
                } else {
                    std::cerr << "Error: argumento posicional inesperado '" << arg << "'.\n";
                    return 1;
                }
            }
        }

        if (options.name.empty()) {
            std::cout << "Uso: mge new <nombre> [--template <3d|2d|headless>] [--dir <carpeta>] [--no-git]\n"
                      << "Ejemplo: mge new mi_juego --template 3d\n";
            return 1;
        }

        std::string message;
        int status = createNewProject(options, exePath, message);
        if (status == 0) {
            std::cout << message;
        } else {
            std::cerr << message << "\n";
        }
        return status;
    }

    int cmdBuild(int argc, char* argv[]) {
        std::string preset = "dev";

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Uso: mge build [opciones]\n\n"
                          << "Compila el proyecto actual mediante presets de CMake.\n\n"
                          << "Opciones:\n"
                          << "  --release      Compila utilizando el preset 'release'\n"
                          << "  --preset <p>   Especifica el preset de compilación (ej: dev, release, ci)\n"
                          << "  -h, --help     Muestra esta ayuda\n";
                return 0;
            }
            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requiere el nombre de un preset.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: opción desconocida '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no se detectó ningún proyecto miniGameEngine en la ruta actual ni en sus "
                         "ancestros.\n";
            return 1;
        }

        // Si la carpeta de build del preset no contiene CMakeCache.txt, configurar previamente
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        return runCommand({"cmake", "--build", "--preset", preset}, proj.rootDir);
    }

    int cmdRun(int argc, char* argv[]) {
        std::string preset = "dev";
        std::vector<std::string> gameArgs;

        bool parsingGameArgs = false;
        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (parsingGameArgs) {
                gameArgs.push_back(std::string(arg));
                continue;
            }

            if (arg == "--") {
                parsingGameArgs = true;
                continue;
            }

            if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Uso: mge run [opciones] [-- <argumentos del juego...>]\n\n"
                    << "Compila y ejecuta el objetivo principal del proyecto.\n\n"
                    << "Opciones:\n"
                    << "  --release      Ejecuta la versión compilada con el preset 'release'\n"
                    << "  --preset <p>   Especifica el preset (por defecto: dev)\n"
                    << "  --             Los argumentos siguientes se transfieren directamente al juego\n"
                    << "  -h, --help     Muestra esta ayuda\n";
                return 0;
            }

            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requiere el nombre de un preset.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: opción desconocida '" << arg
                          << "'. Use '--' para argumentos del juego.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no se detectó ningún proyecto miniGameEngine.\n";
            return 1;
        }

        std::string target = (proj.type == ProjectType::EngineRepo) ? "sandbox" : proj.name;
        if (target.empty()) {
            target = "sandbox";
        }

        // Configurar si aún no se ha generado la caché de CMake
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        // Compilar el objetivo específico
        int buildRc = runCommand({"cmake", "--build", "--preset", preset, "--target", target}, proj.rootDir);
        if (buildRc != 0) {
            return buildRc;
        }

        // Localizar el binario compilado
        std::filesystem::path binary = findExecutableBinary(proj.rootDir, target, preset);
        if (binary.empty() || !std::filesystem::exists(binary)) {
            std::cerr << "Error: no se pudo encontrar el ejecutable '" << target
                      << "' en las carpetas de compilación.\n";
            return 1;
        }

        std::vector<std::string> runArgs = {binary.string()};
        runArgs.insert(runArgs.end(), gameArgs.begin(), gameArgs.end());

        return runCommand(runArgs, proj.rootDir);
    }

    int cmdTest(int argc, char* argv[]) {
        std::string preset = "dev";

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Uso: mge test [opciones]\n\n"
                          << "Ejecuta los tests unitarios mediante CTest y los presets del proyecto.\n\n"
                          << "Opciones:\n"
                          << "  --release      Ejecuta los tests con el preset 'release'\n"
                          << "  --preset <p>   Especifica el preset de tests (por defecto: dev)\n"
                          << "  -h, --help     Muestra esta ayuda\n";
                return 0;
            }
            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requiere el nombre de un preset.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: opción desconocida '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no se detectó ningún proyecto miniGameEngine.\n";
            return 1;
        }

        // Configurar si no existe la caché
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        return runCommand({"ctest", "--preset", preset}, proj.rootDir);
    }

    int cmdCheck(int argc, char* argv[]) {
        bool fix = false;

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Uso: mge check [opciones]\n\n"
                    << "Comprueba formato y directrices de código mediante clang-format y clang-tidy.\n\n"
                    << "Opciones:\n"
                    << "  --fix       Corrige automáticamente errores de formato con clang-format\n"
                    << "  -h, --help  Muestra esta ayuda\n";
                return 0;
            }
            if (arg == "--fix") {
                fix = true;
            } else {
                std::cerr << "Error: opción desconocida '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no se detectó ningún proyecto miniGameEngine.\n";
            return 1;
        }

        // Si estamos dentro del repositorio del motor, delegar en scripts/lint.sh
        if (proj.type == ProjectType::EngineRepo) {
            std::filesystem::path lintScript = proj.rootDir / "scripts" / "lint.sh";
            if (std::filesystem::is_regular_file(lintScript)) {
                std::vector<std::string> args = {lintScript.string()};
                if (fix) {
                    args.push_back("--fix");
                }
                return runCommand(args, proj.rootDir);
            }
        }

        // En proyectos generados, comprobar código con clang-format si está disponible
        if (commandExists("clang-format")) {
            std::vector<std::string> fmtArgs = {"clang-format"};
            if (fix) {
                fmtArgs.push_back("-i");
            } else {
                fmtArgs.push_back("--dry-run");
                fmtArgs.push_back("--Werror");
            }

            std::vector<std::filesystem::path> scanDirs = {proj.rootDir / "src", proj.rootDir / "tests"};

            for (const auto& sDir : scanDirs) {
                if (std::filesystem::is_directory(sDir)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(sDir)) {
                        if (entry.is_regular_file()) {
                            std::string ext = entry.path().extension().string();
                            if (ext == ".cpp" || ext == ".h") {
                                fmtArgs.push_back(entry.path().string());
                            }
                        }
                    }
                }
            }

            if (fmtArgs.size() > 2) {
                std::cout << "Comprobando formato con clang-format...\n";
                int rc = runCommand(fmtArgs, proj.rootDir);
                if (rc != 0) {
                    return rc;
                }
            }
        } else {
            std::cout << "Aviso: clang-format no está instalado en el sistema.\n";
        }

        std::cout << colors::green() << colors::bold() << "OK: comprobaciones de formato completadas."
                  << colors::reset() << "\n";
        return 0;
    }

    int cmdClean(int argc, char* argv[]) {
        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Uso: mge clean\n\n"
                          << "Elimina las carpetas de compilación generadas en el proyecto actual.\n";
                return 0;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no se detectó ningún proyecto miniGameEngine.\n";
            return 1;
        }

        size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(proj.rootDir)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name == "build" || name.rfind("build-", 0) == 0) {
                    std::error_code ec;
                    std::filesystem::remove_all(entry.path(), ec);
                    if (!ec) {
                        std::cout << "Eliminada carpeta: " << name << "\n";
                        ++count;
                    }
                }
            }
        }

        if (count == 0) {
            std::cout << "No se encontraron carpetas de compilación para limpiar.\n";
        }
        return 0;
    }

    void printGeneralHelp() {
        std::cout << "mge - Herramienta de línea de comandos de miniGameEngine\n\n"
                  << "Uso: mge <comando> [opciones]\n\n"
                  << "Comandos disponibles:\n"
                  << "  new       Crea un nuevo proyecto a partir de una plantilla estándar\n"
                  << "  build     Compila el proyecto mediante CMake presets\n"
                  << "  run       Compila y ejecuta el objetivo principal del juego\n"
                  << "  test      Ejecuta los tests del proyecto con CTest\n"
                  << "  check     Verifica el formato de código (usa --fix para corregir)\n"
                  << "  clean     Elimina carpetas de compilación generadas (build*)\n"
                  << "  doctor    Comprueba el estado y versiones de las herramientas del sistema\n\n"
                  << "Opciones globales:\n"
                  << "  -h, --help       Muestra esta ayuda\n"
                  << "  -v, --version    Muestra la versión instalada del motor\n\n"
                  << "Ejecute 'mge <comando> --help' para consultar la ayuda específica de cada comando.\n";
    }

    void printVersion() {
        std::cout << "miniGameEngine " << MGE_VERSION_STRING << "\n";
    }

} // namespace mge::cli
