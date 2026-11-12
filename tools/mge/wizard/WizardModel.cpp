#include "WizardModel.h"

#include <mge/Version.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace mge::cli::wizard {

    namespace {

        std::string getClassName(const std::string& name) {
            return toPascalCase(name);
        }

        std::string getTitle(const std::string& name) {
            return toTitleCase(name);
        }

    } // namespace

    uint64_t hashContent(std::string_view content) {
        // Algoritmo FNV-1a de 64 bits para hashes deterministas multiplataforma
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned char ch : content) {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    std::string hashToHex(uint64_t hash) {
        char buffer[17];
        std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
        return std::string(buffer);
    }

    std::string WizardAnswers::languageString() const {
        switch (language) {
        case LanguageChoice::Cpp: return "cpp";
        case LanguageChoice::C: return "c";
        case LanguageChoice::Python: return "python";
        }
        return "cpp";
    }

    std::string WizardAnswers::templateString() const {
        switch (templateChoice) {
        case TemplateChoice::ThreeD: return "3d";
        case TemplateChoice::TwoD: return "2d";
        case TemplateChoice::Headless: return "headless";
        }
        return "3d";
    }

    std::string WizardAnswers::licenseString() const {
        switch (license) {
        case LicenseChoice::Mit: return "mit";
        case LicenseChoice::Apache2: return "apache";
        case LicenseChoice::None: return "none";
        }
        return "none";
    }

    NewProjectOptions WizardAnswers::toProjectOptions() const {
        NewProjectOptions opt;
        switch (language) {
        case LanguageChoice::Cpp: opt.language = ProjectLanguage::Cpp; break;
        case LanguageChoice::C: opt.language = ProjectLanguage::C; break;
        case LanguageChoice::Python: opt.language = ProjectLanguage::Python; break;
        }
        opt.name = name;
        opt.templateName = templateString();
        opt.targetDir = targetDir;
        opt.audio = audio;
        switch (license) {
        case LicenseChoice::Mit: opt.license = LicenseType::Mit; break;
        case LicenseChoice::Apache2: opt.license = LicenseType::Apache2; break;
        case LicenseChoice::None: opt.license = LicenseType::None; break;
        }
        opt.git = git;
        opt.vscode = vscode;
        opt.noGit = !git;
        return opt;
    }

    WizardModel::WizardModel() {
        validateProjectName(m_answers.name, m_nameError);
    }

    bool WizardModel::canAdvance() const {
        if (m_step == WizardStep::Name) {
            return isNameValid();
        }
        return true;
    }

    void WizardModel::nextStep() {
        if (!canAdvance()) {
            return;
        }

        switch (m_step) {
        case WizardStep::Language: m_step = WizardStep::Name; break;
        case WizardStep::Name: m_step = WizardStep::Template; break;
        case WizardStep::Template:
            if (m_answers.templateChoice == TemplateChoice::Headless) {
                m_answers.audio = false; // Sin audio en modo headless
                m_step = WizardStep::License;
            } else {
                m_step = WizardStep::Audio;
            }
            break;
        case WizardStep::Audio: m_step = WizardStep::License; break;
        case WizardStep::License: m_step = WizardStep::Git; break;
        case WizardStep::Git: m_step = WizardStep::VsCode; break;
        case WizardStep::VsCode: m_step = WizardStep::Summary; break;
        case WizardStep::Summary: m_step = WizardStep::Finished; break;
        case WizardStep::Finished:
        case WizardStep::Cancelled: break;
        }
    }

    void WizardModel::prevStep() {
        switch (m_step) {
        case WizardStep::Language: break;
        case WizardStep::Name:
            if (!m_skipLanguage) {
                m_step = WizardStep::Language;
            }
            break;
        case WizardStep::Template: m_step = WizardStep::Name; break;
        case WizardStep::Audio: m_step = WizardStep::Template; break;
        case WizardStep::License:
            if (m_answers.templateChoice == TemplateChoice::Headless) {
                m_step = WizardStep::Template;
            } else {
                m_step = WizardStep::Audio;
            }
            break;
        case WizardStep::Git: m_step = WizardStep::License; break;
        case WizardStep::VsCode: m_step = WizardStep::Git; break;
        case WizardStep::Summary: m_step = WizardStep::VsCode; break;
        case WizardStep::Finished:
        case WizardStep::Cancelled: break;
        }
    }

    void WizardModel::setName(const std::string& name) {
        m_answers.name = name;
        validateProjectName(m_answers.name, m_nameError);
    }

    void WizardModel::appendNameChar(char c) {
        m_answers.name.push_back(c);
        validateProjectName(m_answers.name, m_nameError);
    }

    void WizardModel::backspaceName() {
        if (!m_answers.name.empty()) {
            m_answers.name.pop_back();
            validateProjectName(m_answers.name, m_nameError);
        }
    }

    bool WizardModel::isNameValid() const {
        return validateProjectName(m_answers.name, m_nameError);
    }

    void WizardModel::selectNextLanguage() {
        switch (m_answers.language) {
        case LanguageChoice::Cpp: setLanguage(LanguageChoice::C); break;
        case LanguageChoice::C: setLanguage(LanguageChoice::Python); break;
        case LanguageChoice::Python: setLanguage(LanguageChoice::Cpp); break;
        }
    }

    void WizardModel::selectPrevLanguage() {
        switch (m_answers.language) {
        case LanguageChoice::Cpp: setLanguage(LanguageChoice::Python); break;
        case LanguageChoice::C: setLanguage(LanguageChoice::Cpp); break;
        case LanguageChoice::Python: setLanguage(LanguageChoice::C); break;
        }
    }

    void WizardModel::setLanguage(LanguageChoice choice) {
        m_answers.language = choice;
        if (choice == LanguageChoice::C) {
            if (m_answers.templateChoice == TemplateChoice::TwoD) {
                m_answers.templateChoice = TemplateChoice::ThreeD;
            }
        } else if (choice == LanguageChoice::Python) {
            if (m_answers.templateChoice == TemplateChoice::Headless) {
                m_answers.templateChoice = TemplateChoice::ThreeD;
                m_answers.audio = true;
            }
        }
    }

    void WizardModel::setSkipLanguage(bool skip) {
        m_skipLanguage = skip;
        if (skip && m_step == WizardStep::Language) {
            m_step = WizardStep::Name;
        }
    }

    void WizardModel::selectNextTemplate() {
        if (m_answers.language == LanguageChoice::C) {
            if (m_answers.templateChoice == TemplateChoice::ThreeD) {
                m_answers.templateChoice = TemplateChoice::Headless;
                m_answers.audio = false;
            } else {
                m_answers.templateChoice = TemplateChoice::ThreeD;
                m_answers.audio = true;
            }
        } else if (m_answers.language == LanguageChoice::Python) {
            if (m_answers.templateChoice == TemplateChoice::ThreeD) {
                m_answers.templateChoice = TemplateChoice::TwoD;
                m_answers.audio = true;
            } else {
                m_answers.templateChoice = TemplateChoice::ThreeD;
                m_answers.audio = true;
            }
        } else {
            switch (m_answers.templateChoice) {
            case TemplateChoice::ThreeD:
                m_answers.templateChoice = TemplateChoice::TwoD;
                m_answers.audio = true;
                break;
            case TemplateChoice::TwoD:
                m_answers.templateChoice = TemplateChoice::Headless;
                m_answers.audio = false;
                break;
            case TemplateChoice::Headless:
                m_answers.templateChoice = TemplateChoice::ThreeD;
                m_answers.audio = true;
                break;
            }
        }
    }

    void WizardModel::selectPrevTemplate() {
        if (m_answers.language == LanguageChoice::C) {
            selectNextTemplate();
        } else if (m_answers.language == LanguageChoice::Python) {
            selectNextTemplate();
        } else {
            switch (m_answers.templateChoice) {
            case TemplateChoice::ThreeD:
                m_answers.templateChoice = TemplateChoice::Headless;
                m_answers.audio = false;
                break;
            case TemplateChoice::TwoD:
                m_answers.templateChoice = TemplateChoice::ThreeD;
                m_answers.audio = true;
                break;
            case TemplateChoice::Headless:
                m_answers.templateChoice = TemplateChoice::TwoD;
                m_answers.audio = true;
                break;
            }
        }
    }

    void WizardModel::setTemplate(TemplateChoice choice) {
        m_answers.templateChoice = choice;
        if (choice == TemplateChoice::Headless) {
            m_answers.audio = false;
        }
    }

    void WizardModel::toggleAudio() {
        if (m_answers.templateChoice != TemplateChoice::Headless) {
            m_answers.audio = !m_answers.audio;
        }
    }

    void WizardModel::setAudio(bool value) {
        if (m_answers.templateChoice != TemplateChoice::Headless) {
            m_answers.audio = value;
        }
    }

    void WizardModel::selectNextLicense() {
        switch (m_answers.license) {
        case LicenseChoice::Mit: m_answers.license = LicenseChoice::Apache2; break;
        case LicenseChoice::Apache2: m_answers.license = LicenseChoice::None; break;
        case LicenseChoice::None: m_answers.license = LicenseChoice::Mit; break;
        }
    }

    void WizardModel::selectPrevLicense() {
        switch (m_answers.license) {
        case LicenseChoice::Mit: m_answers.license = LicenseChoice::None; break;
        case LicenseChoice::Apache2: m_answers.license = LicenseChoice::Mit; break;
        case LicenseChoice::None: m_answers.license = LicenseChoice::Apache2; break;
        }
    }

    void WizardModel::setLicense(LicenseChoice choice) {
        m_answers.license = choice;
    }

    void WizardModel::toggleGit() {
        m_answers.git = !m_answers.git;
    }

    void WizardModel::setGit(bool value) {
        m_answers.git = value;
    }

    void WizardModel::toggleVsCode() {
        m_answers.vscode = !m_answers.vscode;
    }

    void WizardModel::setVsCode(bool value) {
        m_answers.vscode = value;
    }

    std::string generateLicenseText(LicenseChoice license, int year, const std::string& author) {
        std::string authorStr = author.empty() ? "MiniGameEngine Contributor" : author;
        std::string yearStr = std::to_string(year);

        if (license == LicenseChoice::Mit) {
            return "MIT License\n\n"
                   "Copyright (c) " +
                   yearStr + " " + authorStr +
                   "\n\n"
                   "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
                   "of this software and associated documentation files (the \"Software\"), to deal\n"
                   "in the Software without restriction, including without limitation the rights\n"
                   "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
                   "copies of the Software, and to permit persons to whom the Software is\n"
                   "furnished to do so, subject to the following conditions:\n\n"
                   "The above copyright notice and this permission notice shall be included in all\n"
                   "copies or substantial portions of the Software.\n\n"
                   "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
                   "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
                   "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
                   "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
                   "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
                   "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
                   "SOFTWARE.\n";
        }

        if (license == LicenseChoice::Apache2) {
            return "                                 Apache License\n"
                   "                           Version 2.0, January 2004\n"
                   "                        http://www.apache.org/licenses/\n\n"
                   "   Copyright " +
                   yearStr + " " + authorStr +
                   "\n\n"
                   "   Licensed under the Apache License, Version 2.0 (the \"License\");\n"
                   "   you may not use this file except in compliance with the License.\n"
                   "   You may obtain a copy of the License at\n\n"
                   "       http://www.apache.org/licenses/LICENSE-2.0\n\n"
                   "   Unless required by applicable law or agreed to in writing, software\n"
                   "   distributed under the License is distributed on an \"AS IS\" BASIS,\n"
                   "   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
                   "   See the License for the specific language governing permissions and\n"
                   "   limitations under the License.\n";
        }

        return {};
    }

    std::string generateVsCodeTasksJson() {
        return "{\n"
               "    \"version\": \"2.0.0\",\n"
               "    \"tasks\": [\n"
               "        {\n"
               "            \"label\": \"Configure (dev)\",\n"
               "            \"type\": \"shell\",\n"
               "            \"command\": \"cmake --preset dev\",\n"
               "            \"problemMatcher\": []\n"
               "        },\n"
               "        {\n"
               "            \"label\": \"Build (dev)\",\n"
               "            \"type\": \"shell\",\n"
               "            \"command\": \"test -f build/CMakeCache.txt || cmake --preset dev; cmake --build "
               "--preset dev\",\n"
               "            \"group\": { \"kind\": \"build\", \"isDefault\": true },\n"
               "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
               "        },\n"
               "        {\n"
               "            \"label\": \"Run game\",\n"
               "            \"type\": \"shell\",\n"
               "            \"command\": \"mge run\",\n"
               "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
               "        },\n"
               "        {\n"
               "            \"label\": \"Test (dev)\",\n"
               "            \"type\": \"shell\",\n"
               "            \"command\": \"mge test\",\n"
               "            \"group\": { \"kind\": \"test\", \"isDefault\": true },\n"
               "            \"problemMatcher\": { \"base\": \"$gcc\", \"fileLocation\": \"absolute\" }\n"
               "        },\n"
               "        {\n"
               "            \"label\": \"Check (format)\",\n"
               "            \"type\": \"shell\",\n"
               "            \"command\": \"mge check\",\n"
               "            \"problemMatcher\": []\n"
               "        }\n"
               "    ]\n"
               "}\n";
    }

    std::string generateVsCodeSettingsJson() {
        return "{\n"
               "    \"cmake.configureOnOpen\": false\n"
               "}\n";
    }

    std::string generateMgeProject(const WizardAnswers& answers) {
        std::stringstream ss;
        ss << "name=" << answers.name << "\n"
           << "language=" << answers.languageString() << "\n"
           << "template=" << answers.templateString() << "\n"
           << "audio=" << (answers.audio ? "yes" : "no") << "\n"
           << "license=" << answers.licenseString() << "\n";
        return ss.str();
    }

    std::vector<PlannedFile> WizardModel::planFileTree(const std::filesystem::path& tmplBase) const {
        std::vector<PlannedFile> files;

        std::string tmplFolder = m_answers.templateString();
        if (m_answers.language == LanguageChoice::C) {
            tmplFolder = "c-" + tmplFolder;
        } else if (m_answers.language == LanguageChoice::Python) {
            tmplFolder = "python-" + tmplFolder;
        }

        std::filesystem::path tmplDir = tmplBase / tmplFolder;
        if (!std::filesystem::is_directory(tmplDir)) {
            tmplDir = tmplBase / m_answers.templateString();
            if (!std::filesystem::is_directory(tmplDir)) {
                return files;
            }
        }

        std::string className = getClassName(m_answers.name);
        std::string title = getTitle(m_answers.name);
        std::string version = MGE_VERSION_STRING;
        NewProjectOptions options = m_answers.toProjectOptions();

        for (const auto& entry : std::filesystem::recursive_directory_iterator(tmplDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::filesystem::path rel = std::filesystem::relative(entry.path(), tmplDir);
            std::string relStr = rel.generic_string();

            // Reemplazo de comodines en rutas
            size_t pos = 0;
            while ((pos = relStr.find("@GAME_CLASS@", pos)) != std::string::npos) {
                relStr.replace(pos, 12, className);
                pos += className.length();
            }
            pos = 0;
            while ((pos = relStr.find("@GAME_NAME@", pos)) != std::string::npos) {
                relStr.replace(pos, 11, m_answers.name);
                pos += m_answers.name.length();
            }

            if (rel.filename().string() == "Game.cpp.in" || rel.filename().string() == "Game.cpp") {
                pos = relStr.rfind("Game.cpp");
                if (pos != std::string::npos) {
                    relStr.replace(pos, 8, className + ".cpp");
                }
            }

            bool isTemplateIn = false;
            const std::string inSuffix = ".in";
            if (relStr.size() >= inSuffix.size() &&
                relStr.compare(relStr.size() - inSuffix.size(), inSuffix.size(), inSuffix) == 0) {
                isTemplateIn = true;
                relStr = relStr.substr(0, relStr.size() - inSuffix.size());
            }

            std::ifstream file(entry.path(), std::ios::binary);
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            if (isTemplateIn) {
                content = renderTemplateString(content, options, className, title, version);
            }

            files.push_back({std::filesystem::path(relStr), hashContent(content)});
        }

        // Archivo mge.project
        files.push_back({std::filesystem::path("mge.project"), hashContent(generateMgeProject(m_answers))});

        // Archivo LICENSE
        if (m_answers.license != LicenseChoice::None) {
            std::string licText = generateLicenseText(m_answers.license, 2026, "MiniGameEngine Contributor");
            files.push_back({std::filesystem::path("LICENSE"), hashContent(licText)});
        }

        // Configuración de VS Code (.vscode/)
        if (m_answers.vscode) {
            files.push_back(
                {std::filesystem::path(".vscode/tasks.json"), hashContent(generateVsCodeTasksJson())});
            files.push_back(
                {std::filesystem::path(".vscode/settings.json"), hashContent(generateVsCodeSettingsJson())});
        }

        // Ordenar alfabéticamente por ruta relativa
        std::sort(files.begin(), files.end(), [](const PlannedFile& a, const PlannedFile& b) {
            return a.relativePath.generic_string() < b.relativePath.generic_string();
        });

        return files;
    }

} // namespace mge::cli::wizard
