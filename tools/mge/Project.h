#pragma once

#include <filesystem>
#include <string>

namespace mge::cli {

    enum class ProjectType {
        None,
        GameProject,
        EngineRepo
    };

    enum class ProjectLanguage {
        Cpp,
        C,
        Python
    };

    std::string languageToString(ProjectLanguage lang);
    bool parseLanguage(const std::string& str, ProjectLanguage& outLang);
    bool validateLanguageTemplate(ProjectLanguage lang, const std::string& templateName,
                                  std::string& outError);
    const char* minEngineVersionForLanguage(ProjectLanguage lang);
    bool validateLanguageVersion(ProjectLanguage lang, const std::string& engineVersion,
                                 std::string& outError);

    struct ProjectInfo {
        ProjectType type{ProjectType::None};
        std::string name;
        std::string templateType;
        std::filesystem::path rootDir;
        ProjectLanguage language{ProjectLanguage::Cpp};
    };

    // Valida que el nombre cumpla con ^[a-z][a-z0-9_]*$ y no coincida con nombres reservados
    bool validateProjectName(const std::string& name, std::string& outError);

    // Convierte snake_case a PascalCase (ej: space_shooter -> SpaceShooter)
    std::string toPascalCase(const std::string& snakeCase);

    // Convierte snake_case a Title Case (ej: space_shooter -> Space Shooter)
    std::string toTitleCase(const std::string& snakeCase);

    // Reemplaza marcadores @GAME_NAME@, @GAME_CLASS@, @GAME_TITLE@ y @MGE_VERSION@
    std::string renderTemplateString(const std::string& content, const std::string& name,
                                     const std::string& className, const std::string& title,
                                     const std::string& version);

    // Localiza la carpeta de plantillas instaladas o del repositorio
    std::filesystem::path findTemplatesDir(const std::filesystem::path& exePath);

    // Detecta el proyecto inspeccionando la carpeta actual y sus ancestros
    ProjectInfo detectProject(const std::filesystem::path& startDir);

    enum class LicenseType {
        Mit,
        Apache2,
        None
    };

    struct NewProjectOptions {
        std::string name;
        ProjectLanguage language{ProjectLanguage::Cpp};
        std::string templateName{"3d"};
        std::filesystem::path targetDir;
        bool audio{true};
        LicenseType license{LicenseType::Mit};
        bool git{true};
        bool vscode{true};
        bool noGit{false};
        std::string pinnedVersion;
    };

    // Reemplaza marcadores @GAME_NAME@, @GAME_CLASS@, @GAME_TITLE@, @MGE_VERSION@ y módulos opcionales
    std::string renderTemplateString(const std::string& content, const NewProjectOptions& options,
                                     const std::string& className, const std::string& title,
                                     const std::string& version);

    // Crea un proyecto a partir de la plantilla seleccionada
    int createNewProject(const NewProjectOptions& options, const std::filesystem::path& exePath,
                         std::string& outMessage);

} // namespace mge::cli
