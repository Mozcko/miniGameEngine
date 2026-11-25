#include "Commands.h"

#include "Colors.h"
#include "Process.h"
#include "Project.h"
#include "wizard/TerminalView.h"
#include "wizard/WizardModel.h"
#include <mge/Version.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace mge::cli {

    namespace {

        std::string presetBuildConfig(const std::string& preset) {
            if (preset == "release") {
                return "Release";
            }
            if (preset == "minsize") {
                return "MinSizeRel";
            }
            return "Debug";
        }

        std::filesystem::path resolvePresetBuildDir(const std::filesystem::path& rootDir,
                                                    const std::string& preset) {
            std::filesystem::path presetDir = rootDir / ("build-" + preset);
            if (std::filesystem::exists(presetDir / "CMakeCache.txt")) {
                return presetDir;
            }
            std::filesystem::path defaultDir = rootDir / "build";
            if (std::filesystem::exists(defaultDir / "CMakeCache.txt")) {
                return defaultDir;
            }
            return (preset == "dev") ? defaultDir : presetDir;
        }

        bool isMultiConfigBuildDir(const std::filesystem::path& buildDir) {
            std::filesystem::path cache = buildDir / "CMakeCache.txt";
            if (!std::filesystem::is_regular_file(cache)) {
                return false;
            }
            std::ifstream in(cache);
            std::string line;
            while (std::getline(in, line)) {
                if (line.rfind("CMAKE_CONFIGURATION_TYPES:", 0) == 0) {
                    return true;
                }
            }
            return false;
        }

        std::filesystem::path findExecutableBinary(const std::filesystem::path& rootDir,
                                                   const std::string& targetName, const std::string& preset) {
            const std::string configDir = presetBuildConfig(preset);
            std::vector<std::filesystem::path> buildDirs;
            if (preset == "dev") {
                buildDirs = {rootDir / "build", rootDir / ("build-" + preset)};
            } else {
                buildDirs = {rootDir / ("build-" + preset), rootDir / "build"};
            }

            std::vector<std::filesystem::path> candidates;
            for (const auto& bDir : buildDirs) {
#ifdef _WIN32
                candidates.push_back(bDir / (targetName + ".exe"));
                candidates.push_back(bDir / configDir / (targetName + ".exe"));
                candidates.push_back(bDir / targetName);
                candidates.push_back(bDir / configDir / targetName);
#else
                candidates.push_back(bDir / targetName);
                candidates.push_back(bDir / configDir / targetName);
                candidates.push_back(bDir / (targetName + ".exe"));
                candidates.push_back(bDir / configDir / (targetName + ".exe"));
#endif
            }

            for (const auto& path : candidates) {
                if (std::filesystem::is_regular_file(path)) {
                    return path;
                }
            }

            // Recursive search inside build directories
            for (const auto& bDir : buildDirs) {
                if (std::filesystem::is_directory(bDir)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(bDir)) {
                        if (entry.is_regular_file()) {
                            std::string filename = entry.path().filename().string();
#ifdef _WIN32
                            if (filename == targetName + ".exe") {
                                return entry.path();
                            }
#else
                            if (filename == targetName) {
                                return entry.path();
                            }
#endif
                        }
                    }
                }
            }

            return {};
        }

        std::string findPythonExecutable(const std::filesystem::path& rootDir) {
#ifdef _WIN32
            std::filesystem::path venvPyWin = rootDir / ".venv" / "Scripts" / "python.exe";
            if (std::filesystem::is_regular_file(venvPyWin)) {
                return venvPyWin.string();
            }
            if (commandExists("python")) {
                return "python";
            }
            if (commandExists("python3")) {
                return "python3";
            }
            return "python";
#else
            std::filesystem::path venvPy = rootDir / ".venv" / "bin" / "python";
            if (std::filesystem::is_regular_file(venvPy)) {
                return venvPy.string();
            }
            if (commandExists("python3")) {
                return "python3";
            }
            if (commandExists("python")) {
                return "python";
            }
            return "python3";
#endif
        }

    } // namespace

    int cmdNew(int argc, char* argv[], const std::filesystem::path& exePath) {
        NewProjectOptions options;
        bool acceptDefaults = false;
        bool hasExplicitLang = false;
        bool hasExplicitTemplate = false;

        // If called with no arguments
        if (argc == 0) {
            if (isStdinTty() && isStdoutTty()) {
                wizard::WizardModel model;
                if (!wizard::runInteractiveWizard(model)) {
                    return 130;
                }
                options = model.answers().toProjectOptions();
                std::string message;
                int status = createNewProject(options, exePath, message);
                if (status == 0) {
                    std::cout << message;
                } else {
                    std::cerr << message << "\n";
                }
                return status;
            }

            std::cerr << "Error: project name is required in non-interactive environments.\n"
                      << "Usage: mge new <name> [options]\n"
                      << "Run 'mge new --help' to see available options.\n";
            return 1;
        }

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Usage: mge new [name] [options]\n\n"
                    << "Creates a new project from a standard template.\n"
                    << "When run without arguments in an interactive terminal, launches the TUI wizard.\n\n"
                    << "Options:\n"
                    << "  --lang <c|cpp|python>        Project language (default: cpp)\n"
                    << "  --template <3d|2d|headless>  Template to use (default: 3d)\n"
                    << "  --audio / --no-audio         Enable or disable the audio module (default: yes)\n"
                    << "  --license <mit|apache|none>  License to generate (default: mit)\n"
                    << "  --git / --no-git             Initialize a Git repository (default: yes)\n"
                    << "  --vscode / --no-vscode       Configure VS Code tasks in .vscode/ (default: yes)\n"
                    << "  --dir <path>                 Target directory to initialize the project\n"
                    << "  -y, --yes                    Accept default values without prompting\n"
                    << "  -h, --help                   Show this help message\n";
                return 0;
            }
            if (arg == "--lang" || arg == "--language") {
                if (i + 1 < argc) {
                    std::string lStr = argv[++i];
                    ProjectLanguage l = ProjectLanguage::Cpp;
                    if (!parseLanguage(lStr, l)) {
                        std::cerr << "Error: unknown language '" << lStr
                                  << "'. Valid options: cpp, c, python.\n";
                        return 1;
                    }
                    options.language = l;
                    hasExplicitLang = true;
                } else {
                    std::cerr << "Error: --lang requires a language (cpp, c, python).\n";
                    return 1;
                }
            } else if (arg == "--template") {
                if (i + 1 < argc) {
                    options.templateName = argv[++i];
                    hasExplicitTemplate = true;
                } else {
                    std::cerr << "Error: --template requires an argument (3d, 2d, headless).\n";
                    return 1;
                }
            } else if (arg == "--audio") {
                options.audio = true;
            } else if (arg == "--no-audio") {
                options.audio = false;
            } else if (arg == "--license") {
                if (i + 1 < argc) {
                    std::string lic = argv[++i];
                    if (lic == "mit" || lic == "MIT") {
                        options.license = LicenseType::Mit;
                    } else if (lic == "apache" || lic == "apache-2.0" || lic == "Apache-2.0") {
                        options.license = LicenseType::Apache2;
                    } else if (lic == "none" || lic == "None") {
                        options.license = LicenseType::None;
                    } else {
                        std::cerr << "Error: unknown license '" << lic << "'. Use mit, apache, or none.\n";
                        return 1;
                    }
                } else {
                    std::cerr << "Error: --license requires a type (mit, apache, none).\n";
                    return 1;
                }
            } else if (arg == "--git") {
                options.git = true;
                options.noGit = false;
            } else if (arg == "--no-git") {
                options.git = false;
                options.noGit = true;
            } else if (arg == "--vscode") {
                options.vscode = true;
            } else if (arg == "--no-vscode") {
                options.vscode = false;
            } else if (arg == "--yes" || arg == "-y") {
                acceptDefaults = true;
            } else if (arg == "--dir") {
                if (i + 1 < argc) {
                    options.targetDir = argv[++i];
                } else {
                    std::cerr << "Error: --dir requires a path argument.\n";
                    return 1;
                }
            } else if (arg.rfind("-", 0) == 0) {
                std::cerr << "Error: unknown option '" << arg << "'. Run 'mge new --help' for help.\n";
                return 1;
            } else {
                if (options.name.empty()) {
                    options.name = std::string(arg);
                } else {
                    std::cerr << "Error: unexpected positional argument '" << arg << "'.\n";
                    return 1;
                }
            }
        }

        std::string langErr;
        if (!validateLanguageTemplate(options.language, options.templateName, langErr)) {
            std::cerr << "Error: " << langErr << "\n";
            return 1;
        }
        if (!validateLanguageVersion(options.language, MGE_VERSION_STRING, langErr)) {
            std::cerr << "Error: " << langErr << "\n";
            return 1;
        }

        if (options.templateName == "headless") {
            options.audio = false;
        }

        if (options.name.empty()) {
            if (acceptDefaults || !isStdinTty() || !isStdoutTty()) {
                std::cerr << "Error: project name is required.\n"
                          << "Usage: mge new <name> [options]\n";
                return 1;
            }

            // Launch interactive wizard with preconfigured options
            wizard::WizardModel model;
            if (hasExplicitLang) {
                switch (options.language) {
                case ProjectLanguage::Cpp: model.setLanguage(wizard::LanguageChoice::Cpp); break;
                case ProjectLanguage::C: model.setLanguage(wizard::LanguageChoice::C); break;
                case ProjectLanguage::Python: model.setLanguage(wizard::LanguageChoice::Python); break;
                }
                model.setSkipLanguage(true);
            }
            if (hasExplicitTemplate) {
                if (options.templateName == "2d") {
                    model.setTemplate(wizard::TemplateChoice::TwoD);
                } else if (options.templateName == "headless") {
                    model.setTemplate(wizard::TemplateChoice::Headless);
                } else {
                    model.setTemplate(wizard::TemplateChoice::ThreeD);
                }
            }
            model.setAudio(options.audio);
            model.setGit(options.git);
            model.setVsCode(options.vscode);

            if (!wizard::runInteractiveWizard(model)) {
                return 130;
            }
            options = model.answers().toProjectOptions();
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
                std::cout << "Usage: mge build [options]\n\n"
                          << "Builds the current project using CMake presets.\n\n"
                          << "Options:\n"
                          << "  --release      Build using the 'release' preset\n"
                          << "  --preset <p>   Specify the build preset (e.g., dev, release, ci)\n"
                          << "  -h, --help     Show this help message\n";
                return 0;
            }
            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requires a preset name.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no miniGameEngine project detected in the current directory or its "
                         "ancestors.\n";
            return 1;
        }

        if (proj.language == ProjectLanguage::Python) {
            std::string py = findPythonExecutable(proj.rootDir);
            std::filesystem::path pyproject = proj.rootDir / "pyproject.toml";
            std::filesystem::path setupPy = proj.rootDir / "setup.py";
            if (std::filesystem::is_regular_file(pyproject) || std::filesystem::is_regular_file(setupPy)) {
                return runCommand({py, "-m", "pip", "install", "-e", "."}, proj.rootDir);
            }
            std::cout << "Nothing to build for the Python project.\n";
            return 0;
        }

        // If the preset build directory does not contain CMakeCache.txt, configure first
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        std::vector<std::string> buildArgs = {"cmake", "--build", "--preset", preset};
        if (isMultiConfigBuildDir(resolvePresetBuildDir(proj.rootDir, preset))) {
            buildArgs.push_back("--config");
            buildArgs.push_back(presetBuildConfig(preset));
        }
        return runCommand(buildArgs, proj.rootDir);
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
                std::cout << "Usage: mge run [options] [-- <game arguments...>]\n\n"
                          << "Builds and runs the main project target.\n\n"
                          << "Options:\n"
                          << "  --release      Run the version built with the 'release' preset\n"
                          << "  --preset <p>   Specify the preset (default: dev)\n"
                          << "  --             Following arguments are passed directly to the game\n"
                          << "  -h, --help     Show this help message\n";
                return 0;
            }

            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requires a preset name.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: unknown option '" << arg << "'. Use '--' for game arguments.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no miniGameEngine project detected.\n";
            return 1;
        }

        if (proj.language == ProjectLanguage::Python) {
            std::string py = findPythonExecutable(proj.rootDir);
            std::filesystem::path mainPy = proj.rootDir / "main.py";
            if (!std::filesystem::is_regular_file(mainPy)) {
                mainPy = proj.rootDir / "src" / "main.py";
            }
            if (!std::filesystem::is_regular_file(mainPy)) {
                std::cerr << "Error: 'main.py' not found in the Python project.\n";
                return 1;
            }
            std::vector<std::string> runArgs = {py, mainPy.string()};
            runArgs.insert(runArgs.end(), gameArgs.begin(), gameArgs.end());
            return runCommand(runArgs, proj.rootDir);
        }

        std::string target = (proj.type == ProjectType::EngineRepo) ? "sandbox" : proj.name;
        if (target.empty()) {
            target = "sandbox";
        }

        // Configure if the CMake cache has not been generated yet
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        // Build the specific target
        std::vector<std::string> buildArgs = {"cmake", "--build", "--preset", preset, "--target", target};
        if (isMultiConfigBuildDir(resolvePresetBuildDir(proj.rootDir, preset))) {
            buildArgs.push_back("--config");
            buildArgs.push_back(presetBuildConfig(preset));
        }
        int buildRc = runCommand(buildArgs, proj.rootDir);
        if (buildRc != 0) {
            return buildRc;
        }

        // Locate the compiled binary
        std::filesystem::path binary = findExecutableBinary(proj.rootDir, target, preset);
        if (binary.empty() || !std::filesystem::exists(binary)) {
            std::cerr << "Error: could not find executable '" << target << "' in build directories.\n";
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
                std::cout << "Usage: mge test [options]\n\n"
                          << "Runs unit tests using CTest and the project presets.\n\n"
                          << "Options:\n"
                          << "  --release      Run tests with the 'release' preset\n"
                          << "  --preset <p>   Specify the test preset (default: dev)\n"
                          << "  -h, --help     Show this help message\n";
                return 0;
            }
            if (arg == "--release") {
                preset = "release";
            } else if (arg == "--preset") {
                if (i + 1 < argc) {
                    preset = argv[++i];
                } else {
                    std::cerr << "Error: --preset requires a preset name.\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no miniGameEngine project detected.\n";
            return 1;
        }

        if (proj.language == ProjectLanguage::Python) {
            std::string py = findPythonExecutable(proj.rootDir);
            return runCommand({py, "-m", "pytest"}, proj.rootDir);
        }

        // Configure if cache does not exist
        std::filesystem::path cacheA = proj.rootDir / "build" / "CMakeCache.txt";
        std::filesystem::path cacheB = proj.rootDir / ("build-" + preset) / "CMakeCache.txt";
        if (!std::filesystem::exists(cacheA) && !std::filesystem::exists(cacheB)) {
            int cfgRc = runCommand({"cmake", "--preset", preset}, proj.rootDir);
            if (cfgRc != 0) {
                return cfgRc;
            }
        }

        std::vector<std::string> testArgs = {"ctest", "--preset", preset};
        if (isMultiConfigBuildDir(resolvePresetBuildDir(proj.rootDir, preset))) {
            testArgs.push_back("-C");
            testArgs.push_back(presetBuildConfig(preset));
        }
        return runCommand(testArgs, proj.rootDir);
    }

    int cmdCheck(int argc, char* argv[]) {
        bool fix = false;

        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: mge check [options]\n\n"
                          << "Checks code formatting and guidelines using clang-format and clang-tidy.\n\n"
                          << "Options:\n"
                          << "  --fix       Automatically fix formatting errors with clang-format\n"
                          << "  -h, --help  Show this help message\n";
                return 0;
            }
            if (arg == "--fix") {
                fix = true;
            } else {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return 1;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no miniGameEngine project detected.\n";
            return 1;
        }

        // If inside the engine repository, delegate to scripts/lint.sh
        if (proj.type == ProjectType::EngineRepo) {
            std::filesystem::path lintScript = proj.rootDir / "scripts" / "lint.sh";
            if (std::filesystem::is_regular_file(lintScript)) {
#ifdef _WIN32
                if (commandExists("bash")) {
                    std::vector<std::string> args = {"bash", lintScript.string()};
                    if (fix) {
                        args.push_back("--fix");
                    }
                    return runCommand(args, proj.rootDir);
                }
#else
                std::vector<std::string> args = {lintScript.string()};
                if (fix) {
                    args.push_back("--fix");
                }
                return runCommand(args, proj.rootDir);
#endif
            }
        }

        // In generated projects, check code with clang-format if available
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
                            if (ext == ".cpp" || ext == ".h" || ext == ".c") {
                                fmtArgs.push_back(entry.path().string());
                            }
                        }
                    }
                }
            }

            if (fmtArgs.size() > 2) {
                std::cout << "Checking formatting with clang-format...\n";
                int rc = runCommand(fmtArgs, proj.rootDir);
                if (rc != 0) {
                    return rc;
                }
            }
        } else {
            std::cout << "Warning: clang-format is not installed on the system.\n";
        }

        std::cout << colors::green() << colors::bold() << "OK: format checks completed." << colors::reset()
                  << "\n";
        return 0;
    }

    int cmdClean(int argc, char* argv[]) {
        for (int i = 0; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: mge clean\n\n"
                          << "Removes generated build directories in the current project.\n";
                return 0;
            }
        }

        ProjectInfo proj = detectProject(std::filesystem::current_path());
        if (proj.type == ProjectType::None) {
            std::cerr << "Error: no miniGameEngine project detected.\n";
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
                        std::cout << "Removed directory: " << name << "\n";
                        ++count;
                    }
                }
            }
        }

        if (count == 0) {
            std::cout << "No build directories found to clean.\n";
        }
        return 0;
    }

    void printGeneralHelp() {
        std::cout << "mge - miniGameEngine command-line tool\n\n"
                  << "Usage: mge <command> [options]\n\n"
                  << "Available commands:\n"
                  << "  new       Create a new project from a standard template\n"
                  << "  build     Build the project using CMake presets\n"
                  << "  run       Build and run the main game target\n"
                  << "  test      Run project tests with CTest\n"
                  << "  check     Check code formatting (use --fix to auto-format)\n"
                  << "  clean     Remove generated build directories (build*)\n"
                  << "  doctor    Check status and versions of system tools\n\n"
                  << "Global options:\n"
                  << "  -h, --help       Show this help message\n"
                  << "  -v, --version    Show the installed engine version\n\n"
                  << "Run 'mge <command> --help' for command-specific help.\n";
    }

    void printVersion() {
        std::cout << "miniGameEngine " << MGE_VERSION_STRING << "\n";
    }

} // namespace mge::cli
