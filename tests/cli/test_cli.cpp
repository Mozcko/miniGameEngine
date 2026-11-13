#include <doctest/doctest.h>

#include "Colors.h"
#include "Doctor.h"
#include "Process.h"
#include "Project.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace mge::cli;

TEST_CASE("cli: validación de nombres de proyectos") {
    std::string error;

    // Nombres válidos
    CHECK(validateProjectName("space_shooter", error));
    CHECK(error.empty());
    CHECK(validateProjectName("pong", error));
    CHECK(validateProjectName("arcade_2d", error));
    CHECK(validateProjectName("game123", error));
    CHECK(validateProjectName("x", error));

    // Nombres inválidos
    CHECK_FALSE(validateProjectName("", error));
    CHECK_FALSE(error.empty());

    CHECK_FALSE(validateProjectName("1game", error));
    CHECK_FALSE(validateProjectName("_game", error));
    CHECK_FALSE(validateProjectName("MyGame", error));
    CHECK_FALSE(validateProjectName("space-shooter", error));
    CHECK_FALSE(validateProjectName("game with space", error));
    CHECK_FALSE(validateProjectName("game$special", error));

    // Nombres reservados
    CHECK_FALSE(validateProjectName("sandbox", error));
    CHECK_FALSE(validateProjectName("run", error));
    CHECK_FALSE(validateProjectName("mge_core", error));
    CHECK_FALSE(validateProjectName("tools", error));
    CHECK_FALSE(validateProjectName("cmake", error));
}

TEST_CASE("cli: conversión de nombres a PascalCase y Title Case") {
    CHECK(toPascalCase("space_shooter") == "SpaceShooter");
    CHECK(toTitleCase("space_shooter") == "Space Shooter");

    CHECK(toPascalCase("pong") == "Pong");
    CHECK(toTitleCase("pong") == "Pong");

    CHECK(toPascalCase("arcade_plane_2d") == "ArcadePlane2d");
    CHECK(toTitleCase("arcade_plane_2d") == "Arcade Plane 2d");

    CHECK(toPascalCase("a_b_c") == "ABC");
    CHECK(toTitleCase("a_b_c") == "A B C");
}

TEST_CASE("cli: renderizado de plantillas de texto") {
    std::string templateStr =
        "class @GAME_CLASS@ : public App {}; // @GAME_NAME@ - @GAME_TITLE@ (v@MGE_VERSION@)";
    std::string rendered = renderTemplateString(templateStr, "demo_game", "DemoGame", "Demo Game", "0.9.0");

    CHECK(rendered == "class DemoGame : public App {}; // demo_game - Demo Game (v0.9.0)");
}

TEST_CASE("cli: parseLanguage y languageToString") {
    ProjectLanguage lang = ProjectLanguage::Cpp;
    CHECK(parseLanguage("cpp", lang));
    CHECK(lang == ProjectLanguage::Cpp);
    CHECK(languageToString(lang) == "cpp");

    CHECK(parseLanguage("c++", lang));
    CHECK(lang == ProjectLanguage::Cpp);

    CHECK(parseLanguage("c", lang));
    CHECK(lang == ProjectLanguage::C);
    CHECK(languageToString(lang) == "c");

    CHECK(parseLanguage("C", lang));
    CHECK(lang == ProjectLanguage::C);

    CHECK(parseLanguage("python", lang));
    CHECK(lang == ProjectLanguage::Python);
    CHECK(languageToString(lang) == "python");

    CHECK(parseLanguage("py", lang));
    CHECK(lang == ProjectLanguage::Python);

    CHECK_FALSE(parseLanguage("rust", lang));
    CHECK_FALSE(parseLanguage("java", lang));
}

TEST_CASE("cli: validación de plantillas según el lenguaje") {
    std::string err;
    // Cpp: 3d, 2d, headless, window
    CHECK(validateLanguageTemplate(ProjectLanguage::Cpp, "3d", err));
    CHECK(validateLanguageTemplate(ProjectLanguage::Cpp, "2d", err));
    CHECK(validateLanguageTemplate(ProjectLanguage::Cpp, "headless", err));
    CHECK(validateLanguageTemplate(ProjectLanguage::Cpp, "window", err));
    CHECK_FALSE(validateLanguageTemplate(ProjectLanguage::Cpp, "isometric", err));

    // C: 3d, headless
    CHECK(validateLanguageTemplate(ProjectLanguage::C, "3d", err));
    CHECK(validateLanguageTemplate(ProjectLanguage::C, "headless", err));
    CHECK_FALSE(validateLanguageTemplate(ProjectLanguage::C, "2d", err));
    CHECK(err.find("no válida para C") != std::string::npos);

    // Python: 3d, 2d
    CHECK(validateLanguageTemplate(ProjectLanguage::Python, "3d", err));
    CHECK(validateLanguageTemplate(ProjectLanguage::Python, "2d", err));
    CHECK_FALSE(validateLanguageTemplate(ProjectLanguage::Python, "headless", err));
    CHECK(err.find("no válida para Python") != std::string::npos);
}

TEST_CASE("cli: detección de proyectos") {
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "mge_test_proj_detect";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // Sin archivo de proyecto
    auto infoEmpty = detectProject(tempDir);
    CHECK(infoEmpty.type == ProjectType::None);

    // Con archivo mge.project
    {
        std::ofstream proj(tempDir / "mge.project");
        proj << "name=test_game\n";
        proj << "template=2d\n";
    }

    auto infoProj = detectProject(tempDir);
    CHECK(infoProj.type == ProjectType::GameProject);
    CHECK(infoProj.name == "test_game");
    CHECK(infoProj.templateType == "2d");
    CHECK(infoProj.rootDir == tempDir);
    CHECK(infoProj.language == ProjectLanguage::Cpp);

    // Búsqueda desde un subdirectorio anidado
    std::filesystem::path subDir = tempDir / "src" / "deep";
    std::filesystem::create_directories(subDir);

    auto infoSub = detectProject(subDir);
    CHECK(infoSub.type == ProjectType::GameProject);
    CHECK(infoSub.name == "test_game");
    CHECK(infoSub.templateType == "2d");
    CHECK(infoSub.rootDir == tempDir);

    // Con archivo mge.project indicando language=python
    {
        std::ofstream proj(tempDir / "mge.project");
        proj << "name=py_game\n";
        proj << "template=2d\n";
        proj << "language=python\n";
    }
    auto infoPy = detectProject(tempDir);
    CHECK(infoPy.type == ProjectType::GameProject);
    CHECK(infoPy.language == ProjectLanguage::Python);

    // Con archivo mge.project indicando language=c
    {
        std::ofstream proj(tempDir / "mge.project");
        proj << "name=c_game\n";
        proj << "template=3d\n";
        proj << "language=c\n";
    }
    auto infoC = detectProject(tempDir);
    CHECK(infoC.type == ProjectType::GameProject);
    CHECK(infoC.language == ProjectLanguage::C);

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("cli: diagnósticos doctor") {
    auto items = runDoctorChecks();
    CHECK(items.size() == 7);

    bool foundCompiler = false;
    bool foundCmake = false;
    bool foundNinja = false;
    bool foundSdl3 = false;
    bool foundGlslang = false;
    bool foundVulkan = false;
    bool foundGlm = false;

    for (const auto& item : items) {
        if (item.name == "Compilador C++") {
            foundCompiler = true;
            CHECK(item.required);
        } else if (item.name == "CMake") {
            foundCmake = true;
            CHECK(item.required);
        } else if (item.name == "Ninja") {
            foundNinja = true;
            CHECK_FALSE(item.required); // Ninja es opcional
        } else if (item.name == "SDL3") {
            foundSdl3 = true;
            CHECK(item.required);
        } else if (item.name == "glslang") {
            foundGlslang = true;
            CHECK(item.required);
        } else if (item.name == "Vulkan Loader & ICD") {
            foundVulkan = true;
            CHECK(item.required);
        } else if (item.name == "Cabeceras GLM") {
            foundGlm = true;
            CHECK(item.required);
        }
    }

    CHECK(foundCompiler);
    CHECK(foundCmake);
    CHECK(foundNinja);
    CHECK(foundSdl3);
    CHECK(foundGlslang);
    CHECK(foundVulkan);
    CHECK(foundGlm);
}

TEST_CASE("cli: ejecución y captura de procesos") {
    CHECK(commandExists("cmake"));
    CHECK_FALSE(commandExists("comando_que_definitivamente_no_existe_12345"));

    std::string out;
    int rc = runCommandCapture({"echo", "mge_cli_test_output"}, out);
    CHECK(rc == 0);
    CHECK(out.find("mge_cli_test_output") != std::string::npos);
}

TEST_CASE("cli: diagnósticos doctor para Python") {
    auto items = runPythonDoctorChecks();
    CHECK(items.size() == 4);
    CHECK(items[0].name.find("Python") != std::string::npos);
    CHECK(items[1].name == "pip");
    CHECK(items[2].name == "venv");
    CHECK(items[3].name.find("mge") != std::string::npos);
}

TEST_CASE("cli: validación de versión mínima por lenguaje") {
    std::string err;
    CHECK_FALSE(validateLanguageVersion(ProjectLanguage::C, "0.9.0", err));
    CHECK(err.find("1.0.0") != std::string::npos);
    CHECK_FALSE(validateLanguageVersion(ProjectLanguage::Python, "0.9.9", err));
    CHECK(err.find("1.0.0") != std::string::npos);

    CHECK(validateLanguageVersion(ProjectLanguage::C, "1.0.0", err));
    CHECK(validateLanguageVersion(ProjectLanguage::Python, "1.0.0", err));
    CHECK(validateLanguageVersion(ProjectLanguage::Python, "1.2.0", err));
    CHECK(validateLanguageVersion(ProjectLanguage::Cpp, "0.9.0", err));

    NewProjectOptions opts;
    opts.name = "bad_ver_c";
    opts.language = ProjectLanguage::C;
    opts.templateName = "3d";
    opts.targetDir = std::filesystem::temp_directory_path() / "mge_test_bad_ver_c";
    opts.noGit = true;
    opts.git = false;
    opts.vscode = false;
    opts.pinnedVersion = "0.9.0";
    std::filesystem::remove_all(opts.targetDir);

    std::string msg;
    CHECK(createNewProject(opts, "", msg) == 1);
    CHECK(msg.find("1.0.0") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(opts.targetDir));
}
