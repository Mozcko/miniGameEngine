#include <doctest/doctest.h>

#include "Project.h"
#include "wizard/TerminalView.h"
#include "wizard/WizardModel.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace mge::cli;
using namespace mge::cli::wizard;

namespace {

    bool updateGoldens() {
        const char* env = std::getenv("MGE_UPDATE_GOLDEN");
        return env && std::string(env) == "1";
    }

    std::string formatSnapshot(const std::vector<PlannedFile>& files) {
        std::stringstream ss;
        for (const auto& file : files) {
            ss << file.relativePath.generic_string() << " " << hashToHex(file.contentHash) << "\n";
        }
        return ss.str();
    }

    void verifyGolden(const std::string& goldenName, const std::string& actualSnapshot) {
        std::filesystem::path goldenDir(MGE_CLI_GOLDEN_DIR);
        std::filesystem::create_directories(goldenDir);
        std::filesystem::path goldenFile = goldenDir / (goldenName + ".golden");

        if (updateGoldens() || !std::filesystem::exists(goldenFile)) {
            std::ofstream out(goldenFile);
            out << actualSnapshot;
        }

        std::ifstream in(goldenFile);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string expected = buffer.str();

        CHECK(actualSnapshot == expected);
    }

} // namespace

TEST_CASE("wizard: navegación y máquina de estados del modelo") {
    WizardModel model;
    CHECK(model.currentStep() == WizardStep::Language);
    CHECK(model.canAdvance());

    model.selectNextLanguage();
    CHECK(model.answers().language == LanguageChoice::C);
    model.selectNextLanguage();
    CHECK(model.answers().language == LanguageChoice::Python);
    model.selectNextLanguage();
    CHECK(model.answers().language == LanguageChoice::Cpp);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Name);
    CHECK_FALSE(model.canAdvance());

    model.setName("space_shooter");
    CHECK(model.canAdvance());
    CHECK(model.isNameValid());

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Template);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Audio);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::License);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Git);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::VsCode);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Summary);

    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Finished);
    CHECK(model.isFinished());
}

TEST_CASE("wizard: modo headless omite pregunta de audio") {
    WizardModel model;
    model.nextStep();
    model.setName("sim_server");
    model.nextStep();
    CHECK(model.currentStep() == WizardStep::Template);

    model.setTemplate(TemplateChoice::Headless);
    CHECK(model.answers().templateString() == "headless");
    CHECK_FALSE(model.answers().audio);

    // Al avanzar desde Template en modo Headless, salta Audio y va directo a License
    model.nextStep();
    CHECK(model.currentStep() == WizardStep::License);

    // Al retroceder desde License en modo Headless, regresa a Template
    model.prevStep();
    CHECK(model.currentStep() == WizardStep::Template);
}

TEST_CASE("wizard: selección cíclica de plantillas y licencias") {
    WizardModel model;
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);

    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::TwoD);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::Headless);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);

    model.selectPrevTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::Headless);
    model.selectPrevTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::TwoD);

    CHECK(model.answers().license == LicenseChoice::Mit);
    model.selectNextLicense();
    CHECK(model.answers().license == LicenseChoice::Apache2);
    model.selectNextLicense();
    CHECK(model.answers().license == LicenseChoice::None);
    model.selectNextLicense();
    CHECK(model.answers().license == LicenseChoice::Mit);

    model.selectPrevLicense();
    CHECK(model.answers().license == LicenseChoice::None);
}

TEST_CASE("wizard: mutadores y cancelación") {
    WizardModel model;
    model.appendNameChar('m');
    model.appendNameChar('y');
    model.appendNameChar('_');
    model.appendNameChar('g');
    CHECK(model.answers().name == "my_g");

    model.backspaceName();
    CHECK(model.answers().name == "my_");

    CHECK(model.answers().git);
    model.toggleGit();
    CHECK_FALSE(model.answers().git);

    CHECK(model.answers().vscode);
    model.toggleVsCode();
    CHECK_FALSE(model.answers().vscode);

    CHECK(model.answers().audio);
    model.toggleAudio();
    CHECK_FALSE(model.answers().audio);

    model.cancel();
    CHECK(model.currentStep() == WizardStep::Cancelled);
    CHECK(model.isCancelled());
}

TEST_CASE("wizard: filtrado de plantillas según el lenguaje") {
    WizardModel model;
    // C solo soporta 3D y Headless
    model.setLanguage(LanguageChoice::C);
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::Headless);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);
    model.selectPrevTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::Headless);

    // Python solo soporta 3D y 2D
    model.setLanguage(LanguageChoice::Python);
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::TwoD);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);
    model.selectPrevTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::TwoD);

    // Cpp soporta 3D, 2D y Headless
    model.setLanguage(LanguageChoice::Cpp);
    CHECK(model.answers().templateChoice == TemplateChoice::TwoD);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::Headless);
    model.selectNextTemplate();
    CHECK(model.answers().templateChoice == TemplateChoice::ThreeD);
}

TEST_CASE("wizard: snapshot tests del árbol de archivos con golden files") {
    std::filesystem::path tmplBase(MGE_SOURCE_TEMPLATES_DIR);
    REQUIRE(std::filesystem::is_directory(tmplBase));

    // 1. 3D por defecto (audio: on, license: MIT, git: on, vscode: on)
    {
        WizardModel model;
        model.setName("space_shooter");
        model.setTemplate(TemplateChoice::ThreeD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("3d_default", formatSnapshot(files));
    }

    // 2. 2D por defecto (audio: on, license: MIT, git: on, vscode: on)
    {
        WizardModel model;
        model.setName("arcade_plane");
        model.setTemplate(TemplateChoice::TwoD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("2d_default", formatSnapshot(files));
    }

    // 3. Headless por defecto (audio: off, license: MIT, git: on, vscode: on)
    {
        WizardModel model;
        model.setName("sim_core");
        model.setTemplate(TemplateChoice::Headless);
        model.setAudio(false);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("headless_default", formatSnapshot(files));
    }

    // 4. 3D sin audio, licencia Apache, sin git ni vscode
    {
        WizardModel model;
        model.setName("quiet_box");
        model.setTemplate(TemplateChoice::ThreeD);
        model.setAudio(false);
        model.setLicense(LicenseChoice::Apache2);
        model.setGit(false);
        model.setVsCode(false);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("3d_no_audio", formatSnapshot(files));
    }

    // 5. 2D sin git, sin vscode, sin licencia
    {
        WizardModel model;
        model.setName("plain_2d");
        model.setTemplate(TemplateChoice::TwoD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::None);
        model.setGit(false);
        model.setVsCode(false);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("2d_no_git_no_vscode", formatSnapshot(files));
    }

    // 6. C 3D
    {
        WizardModel model;
        model.setLanguage(LanguageChoice::C);
        model.setName("c_game");
        model.setTemplate(TemplateChoice::ThreeD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("c_3d", formatSnapshot(files));
    }

    // 7. C Headless
    {
        WizardModel model;
        model.setLanguage(LanguageChoice::C);
        model.setName("c_headless");
        model.setTemplate(TemplateChoice::Headless);
        model.setAudio(false);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("c_headless", formatSnapshot(files));
    }

    // 8. Python 3D
    {
        WizardModel model;
        model.setLanguage(LanguageChoice::Python);
        model.setName("py_3d");
        model.setTemplate(TemplateChoice::ThreeD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("python_3d", formatSnapshot(files));
    }

    // 9. Python 2D
    {
        WizardModel model;
        model.setLanguage(LanguageChoice::Python);
        model.setName("py_2d");
        model.setTemplate(TemplateChoice::TwoD);
        model.setAudio(true);
        model.setLicense(LicenseChoice::Mit);
        model.setGit(true);
        model.setVsCode(true);

        auto files = model.planFileTree(tmplBase);
        CHECK_FALSE(files.empty());
        verifyGolden("python_2d", formatSnapshot(files));
    }
}

TEST_CASE("wizard: EOF cancela el asistente de inmediato sin crear archivos") {
    WizardModel model;
    std::filesystem::path tempProjectDir = std::filesystem::current_path() / "test_wizard_eof_cancel";
    std::filesystem::remove_all(tempProjectDir);
    model.setTargetDir(tempProjectDir);

    auto fakeKeySource = [](char& /*outChar*/) -> Key { return Key::Eof; };

    bool ok = runInteractiveWizard(model, fakeKeySource);
    CHECK_FALSE(ok);
    CHECK(model.isCancelled());
    CHECK_FALSE(std::filesystem::exists(tempProjectDir));
}

TEST_CASE("wizard: EOF a mitad del flujo cancela el asistente sin crear archivos") {
    WizardModel model;
    std::filesystem::path tempProjectDir = std::filesystem::current_path() / "test_wizard_eof_midway";
    std::filesystem::remove_all(tempProjectDir);
    model.setTargetDir(tempProjectDir);

    // Secuencia: selecciona lenguaje (Enter), escribe 'j', 'u', 'e', 'g', 'o', Enter, luego EOF
    std::vector<std::pair<Key, char>> script = {{Key::Enter, '\n'}, {Key::Char, 'j'}, {Key::Char, 'u'},
                                                {Key::Char, 'e'},   {Key::Char, 'g'}, {Key::Char, 'o'},
                                                {Key::Enter, '\n'}, {Key::Eof, 0}};
    size_t keyIndex = 0;
    auto fakeKeySource = [&script, &keyIndex](char& outChar) -> Key {
        if (keyIndex < script.size()) {
            auto item = script[keyIndex++];
            outChar = item.second;
            return item.first;
        }
        return Key::Eof;
    };

    bool ok = runInteractiveWizard(model, fakeKeySource);
    CHECK_FALSE(ok);
    CHECK(model.isCancelled());
    CHECK_FALSE(std::filesystem::exists(tempProjectDir));
}

TEST_CASE("wizard: ESC solitario no bloquea ni cancela el asistente") {
    WizardModel model;
    std::filesystem::path tempProjectDir = std::filesystem::current_path() / "test_wizard_esc_continue";
    std::filesystem::remove_all(tempProjectDir);
    model.setTargetDir(tempProjectDir);

    std::vector<std::pair<Key, char>> script = {
        {Key::Escape, 0},   // ESC solitario ignorado
        {Key::Enter, '\n'}, // Lenguaje C++ por defecto
        {Key::Char, 'm'},   {Key::Char, 'i'},   {Key::Char, 'n'},
        {Key::Char, 'i'},   {Key::Enter, '\n'}, {Key::Escape, 0}, // ESC solitario en selección de plantilla
        {Key::Enter, '\n'},                                       // Plantilla 3D por defecto
        {Key::Enter, '\n'},                                       // Audio por defecto
        {Key::Enter, '\n'},                                       // Licencia por defecto
        {Key::Enter, '\n'},                                       // Git por defecto
        {Key::Enter, '\n'},                                       // VS Code por defecto
        {Key::Enter, '\n'}                                        // Confirmación
    };
    size_t keyIndex = 0;
    auto fakeKeySource = [&script, &keyIndex](char& outChar) -> Key {
        if (keyIndex < script.size()) {
            auto item = script[keyIndex++];
            outChar = item.second;
            return item.first;
        }
        return Key::Eof;
    };

    bool ok = runInteractiveWizard(model, fakeKeySource);
    CHECK(ok);
    CHECK(model.isFinished());
    CHECK(model.answers().name == "mini");
}
