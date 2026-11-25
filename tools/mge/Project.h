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

    // Validates that the name matches ^[a-z][a-z0-9_]*$ and is not a reserved name
    bool validateProjectName(const std::string& name, std::string& outError);

    // Converts snake_case to PascalCase (e.g., space_shooter -> SpaceShooter)
    std::string toPascalCase(const std::string& snakeCase);

    // Converts snake_case to Title Case (e.g., space_shooter -> Space Shooter)
    std::string toTitleCase(const std::string& snakeCase);

    // Replaces @GAME_NAME@, @GAME_CLASS@, @GAME_TITLE@, and @MGE_VERSION@ placeholders
    std::string renderTemplateString(const std::string& content, const std::string& name,
                                     const std::string& className, const std::string& title,
                                     const std::string& version);

    // Locates the installed or repository templates directory
    std::filesystem::path findTemplatesDir(const std::filesystem::path& exePath);

    // Detects the project by inspecting the current directory and its ancestors
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

    // Replaces @GAME_NAME@, @GAME_CLASS@, @GAME_TITLE@, @MGE_VERSION@, and optional module placeholders
    std::string renderTemplateString(const std::string& content, const NewProjectOptions& options,
                                     const std::string& className, const std::string& title,
                                     const std::string& version);

    // Creates a project from the selected template
    int createNewProject(const NewProjectOptions& options, const std::filesystem::path& exePath,
                         std::string& outMessage);

} // namespace mge::cli
