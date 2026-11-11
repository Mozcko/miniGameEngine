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

    // Búsqueda desde un subdirectorio anidado
    std::filesystem::path subDir = tempDir / "src" / "deep";
    std::filesystem::create_directories(subDir);

    auto infoSub = detectProject(subDir);
    CHECK(infoSub.type == ProjectType::GameProject);
    CHECK(infoSub.name == "test_game");
    CHECK(infoSub.templateType == "2d");
    CHECK(infoSub.rootDir == tempDir);

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
