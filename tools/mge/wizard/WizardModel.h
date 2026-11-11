#pragma once

#include "../Project.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mge::cli::wizard {

    enum class TemplateChoice {
        ThreeD,
        TwoD,
        Headless
    };

    enum class LicenseChoice {
        Mit,
        Apache2,
        None
    };

    enum class WizardStep {
        Name,
        Template,
        Audio,
        License,
        Git,
        VsCode,
        Summary,
        Finished,
        Cancelled
    };

    struct WizardAnswers {
        std::string name;
        TemplateChoice templateChoice{TemplateChoice::ThreeD};
        bool audio{true};
        LicenseChoice license{LicenseChoice::Mit};
        bool git{true};
        bool vscode{true};
        std::filesystem::path targetDir;

        std::string templateString() const;
        std::string licenseString() const;
        NewProjectOptions toProjectOptions() const;
    };

    struct PlannedFile {
        std::filesystem::path relativePath;
        uint64_t contentHash{0};
    };

    class WizardModel {
    public:
        WizardModel();

        WizardStep currentStep() const { return m_step; }
        void setStep(WizardStep step) { m_step = step; }

        bool canAdvance() const;
        void nextStep();
        void prevStep();
        void cancel() { m_step = WizardStep::Cancelled; }

        bool isFinished() const { return m_step == WizardStep::Finished; }
        bool isCancelled() const { return m_step == WizardStep::Cancelled; }

        const WizardAnswers& answers() const { return m_answers; }
        WizardAnswers& answers() { return m_answers; }

        // Modificadores de campos
        void setName(const std::string& name);
        void appendNameChar(char c);
        void backspaceName();

        void selectNextTemplate();
        void selectPrevTemplate();
        void setTemplate(TemplateChoice choice);

        void toggleAudio();
        void setAudio(bool value);

        void selectNextLicense();
        void selectPrevLicense();
        void setLicense(LicenseChoice choice);

        void toggleGit();
        void setGit(bool value);

        void toggleVsCode();
        void setVsCode(bool value);

        // Validación de nombres
        bool isNameValid() const;
        std::string nameError() const { return m_nameError; }

        // Planificación determinista del árbol de archivos para pruebas snapshot
        std::vector<PlannedFile> planFileTree(const std::filesystem::path& tmplBase) const;

    private:
        WizardStep m_step{WizardStep::Name};
        WizardAnswers m_answers;
        mutable std::string m_nameError;
    };

    // Funciones de utilidad para licencias, VS Code y metadata
    uint64_t hashContent(std::string_view content);
    std::string hashToHex(uint64_t hash);
    std::string generateLicenseText(LicenseChoice license, int year, const std::string& author);
    std::string generateVsCodeTasksJson();
    std::string generateVsCodeSettingsJson();
    std::string generateMgeProject(const WizardAnswers& answers);

} // namespace mge::cli::wizard
