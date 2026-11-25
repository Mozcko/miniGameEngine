#include "TerminalView.h"

#include "../Colors.h"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace mge::cli::wizard {

    namespace {

#ifndef _WIN32
        // Global state to restore terminal mode in signal handlers (async-signal-safe)
        static struct termios sOrigTerm{};
        static volatile sig_atomic_t sOrigTermSaved{0};
        static struct sigaction sOldSigterm{};
        static struct sigaction sOldSighup{};
        static struct sigaction sOldSigint{};

        extern "C" void terminalSignalHandler(int sig) {
            if (sOrigTermSaved) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &sOrigTerm);
            }
            struct sigaction dfl{};
            dfl.sa_handler = SIG_DFL;
            sigemptyset(&dfl.sa_mask);
            dfl.sa_flags = 0;
            sigaction(sig, &dfl, nullptr);
            raise(sig);
        }

        bool pollInput(int fd, int timeoutMs) {
            struct pollfd pfd{};
            pfd.fd = fd;
            pfd.events = POLLIN;
            while (true) {
                int ret = poll(&pfd, 1, timeoutMs);
                if (ret < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                return (ret > 0) && ((pfd.revents & POLLIN) != 0);
            }
        }
#endif

        class TerminalGuard {
        public:
            explicit TerminalGuard(bool enable = true) {
                if (!enable) {
                    return;
                }
#ifdef _WIN32
                m_hIn = GetStdHandle(STD_INPUT_HANDLE);
                if (m_hIn != INVALID_HANDLE_VALUE && m_hIn != nullptr &&
                    GetConsoleMode(m_hIn, &m_origInMode) != 0) {
                    DWORD mode = m_origInMode;
                    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                    SetConsoleMode(m_hIn, mode);
                    m_active = true;
                }
                m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (m_hOut != INVALID_HANDLE_VALUE && m_hOut != nullptr &&
                    GetConsoleMode(m_hOut, &m_origOutMode) != 0) {
                    if ((m_origOutMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
                        if (SetConsoleMode(m_hOut, m_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
                            m_outModeChanged = true;
                        }
                    }
                }
#else
                if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
                    if (tcgetattr(STDIN_FILENO, &m_origTerm) == 0) {
                        sOrigTerm = m_origTerm;
                        sOrigTermSaved = 1;

                        struct sigaction sa{};
                        sa.sa_handler = terminalSignalHandler;
                        sigemptyset(&sa.sa_mask);
                        sa.sa_flags = 0;
                        sigaction(SIGTERM, &sa, &sOldSigterm);
                        sigaction(SIGHUP, &sa, &sOldSighup);
                        sigaction(SIGINT, &sa, &sOldSigint);

                        struct termios raw = m_origTerm;
                        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
                        raw.c_iflag &= ~(IXON | ICRNL);
                        raw.c_cc[VMIN] = 1;
                        raw.c_cc[VTIME] = 0;
                        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
                        m_active = true;
                    }
                }
#endif
            }

            ~TerminalGuard() { restore(); }

            void restore() {
                if (!m_active) {
                    return;
                }
#ifdef _WIN32
                if (m_hIn != INVALID_HANDLE_VALUE && m_hIn != nullptr) {
                    SetConsoleMode(m_hIn, m_origInMode);
                }
                if (m_outModeChanged && m_hOut != INVALID_HANDLE_VALUE && m_hOut != nullptr) {
                    SetConsoleMode(m_hOut, m_origOutMode);
                    m_outModeChanged = false;
                }
#else
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_origTerm);
                if (sOrigTermSaved) {
                    sigaction(SIGTERM, &sOldSigterm, nullptr);
                    sigaction(SIGHUP, &sOldSighup, nullptr);
                    sigaction(SIGINT, &sOldSigint, nullptr);
                    sOrigTermSaved = 0;
                }
#endif
                m_active = false;
            }

            bool isActive() const { return m_active; }

        private:
            bool m_active{false};
#ifdef _WIN32
            HANDLE m_hIn{INVALID_HANDLE_VALUE};
            DWORD m_origInMode{0};
            HANDLE m_hOut{INVALID_HANDLE_VALUE};
            DWORD m_origOutMode{0};
            bool m_outModeChanged{false};
#else
            struct termios m_origTerm{};
#endif
        };

        Key readKeyEvent(char& outChar) {
#ifdef _WIN32
            int c = _getch();
            if (c == EOF || c == 4) {
                return Key::Eof;
            }
            if (c == 3) {
                return Key::CtrlC;
            }
            if (c == '\r' || c == '\n') {
                return Key::Enter;
            }
            if (c == '\b') {
                return Key::Backspace;
            }
            if (c == '\t') {
                return Key::Tab;
            }
            if (c == ' ') {
                return Key::Space;
            }
            if (c == 0 || c == 224) {
                int arrow = _getch();
                switch (arrow) {
                case 72: return Key::Up;
                case 80: return Key::Down;
                case 75: return Key::Left;
                case 77: return Key::Right;
                default: break;
                }
                return Key::None;
            }
            if (c == 27) {
                return Key::Escape;
            }
            if (c >= 32 && c < 127) {
                outChar = static_cast<char>(c);
                return Key::Char;
            }
            return Key::None;
#else
            char c = 0;
            while (true) {
                ssize_t n = read(STDIN_FILENO, &c, 1);
                if (n == 0) {
                    return Key::Eof;
                }
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return Key::Eof;
                }
                break;
            }

            if (c == 0x03) {
                return Key::CtrlC;
            }
            if (c == 0x04) {
                return Key::Eof;
            }
            if (c == '\r' || c == '\n') {
                return Key::Enter;
            }
            if (c == 0x7f || c == '\b') {
                return Key::Backspace;
            }
            if (c == '\t') {
                return Key::Tab;
            }
            if (c == ' ') {
                return Key::Space;
            }

            if (c == 0x1b) {
                if (!pollInput(STDIN_FILENO, 50)) {
                    // Standalone ESC: does not block or wait for more bytes
                    return Key::Escape;
                }
                char seq[2] = {0, 0};
                while (true) {
                    ssize_t n0 = read(STDIN_FILENO, &seq[0], 1);
                    if (n0 == 0) {
                        return Key::Eof;
                    }
                    if (n0 < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        return Key::Eof;
                    }
                    break;
                }
                if (!pollInput(STDIN_FILENO, 50)) {
                    return Key::Escape;
                }
                while (true) {
                    ssize_t n1 = read(STDIN_FILENO, &seq[1], 1);
                    if (n1 == 0) {
                        return Key::Eof;
                    }
                    if (n1 < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        return Key::Eof;
                    }
                    break;
                }

                if (seq[0] == '[' || seq[0] == 'O') {
                    switch (seq[1]) {
                    case 'A': return Key::Up;
                    case 'B': return Key::Down;
                    case 'C': return Key::Right;
                    case 'D': return Key::Left;
                    }
                }
                return Key::None;
            }

            if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127) {
                outChar = c;
                return Key::Char;
            }
            return Key::None;
#endif
        }

        Key readKey(const KeyReader& customReader, char& outChar) {
            if (customReader) {
                return customReader(outChar);
            }
            return readKeyEvent(outChar);
        }

        void clearCurrentLine() {
            std::cout << "\r\033[K" << std::flush;
        }

        void moveCursorUp(int lines) {
            if (lines > 0) {
                std::cout << "\033[" << lines << "A" << std::flush;
            }
        }

    } // namespace

    bool runInteractiveWizard(WizardModel& model, const KeyReader& customKeyReader) {
        TerminalGuard guard(!customKeyReader);

        std::cout << colors::cyan() << colors::bold() << "mge new - Interactive project wizard"
                  << colors::reset() << "\n\n";

        // Question 0: Project language (skipped if --lang was specified)
        if (!model.skipsLanguage()) {
            const std::vector<std::pair<LanguageChoice, std::string>> languageOptions = {
                {LanguageChoice::Cpp, "C++ (full engine, maximum performance and control)"},
                {LanguageChoice::C, "C (stable C99 API, interoperability and simplicity)"},
                {LanguageChoice::Python, "Python (rapid development with nanobind bindings and pytest)"}};

            int renderedLines = 0;
            while (true) {
                if (renderedLines > 0) {
                    moveCursorUp(renderedLines);
                }

                std::cout << "\r" << colors::bold() << "? Project language:" << colors::reset() << "\033[K\n";
                for (const auto& opt : languageOptions) {
                    std::cout << "\033[K";
                    if (model.answers().language == opt.first) {
                        std::cout << colors::cyan() << colors::bold() << "  ❯ " << opt.second
                                  << colors::reset() << "\n";
                    } else {
                        std::cout << colors::gray() << "    " << opt.second << colors::reset() << "\n";
                    }
                }
                renderedLines = 1 + static_cast<int>(languageOptions.size());
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Up || key == Key::Left) {
                    model.selectPrevLanguage();
                } else if (key == Key::Down || key == Key::Right || key == Key::Tab) {
                    model.selectNextLanguage();
                } else if (key == Key::Enter) {
                    if (renderedLines > 0) {
                        moveCursorUp(renderedLines);
                    }
                    for (int i = 0; i < renderedLines; ++i) {
                        clearCurrentLine();
                        std::cout << "\n";
                    }
                    moveCursorUp(renderedLines);
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " Project language: " << colors::reset() << colors::cyan()
                              << model.answers().languageString() << colors::reset() << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Question 1: Project name with live validation
        {
            while (true) {
                clearCurrentLine();
                std::cout << colors::bold() << "? Project name: " << colors::reset() << model.answers().name;

                if (model.answers().name.empty()) {
                    std::cout << colors::gray() << " (e.g., my_game)" << colors::reset();
                } else if (model.isNameValid()) {
                    std::cout << colors::green() << " [✓]" << colors::reset();
                } else {
                    std::cout << colors::red() << " [✗ " << model.nameError() << "]" << colors::reset();
                }
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Enter) {
                    if (model.isNameValid()) {
                        clearCurrentLine();
                        std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                                  << " Project name: " << colors::reset() << model.answers().name << "\n";
                        model.nextStep();
                        break;
                    }
                } else if (key == Key::Backspace) {
                    model.backspaceName();
                } else if (key == Key::Char) {
                    model.appendNameChar(ch);
                }
            }
        }

        // Question 2: Template (filtered by language)
        {
            std::vector<std::pair<TemplateChoice, std::string>> templateOptions;
            if (model.answers().language == LanguageChoice::C) {
                templateOptions = {
                    {TemplateChoice::ThreeD, "3D (procedural 3D world with lighting and audio)"},
                    {TemplateChoice::Headless, "Headless (deterministic simulation without window or GPU)"}};
            } else if (model.answers().language == LanguageChoice::Python) {
                templateOptions = {
                    {TemplateChoice::ThreeD, "3D (procedural 3D world with camera, cube, and audio)"},
                    {TemplateChoice::TwoD, "2D (2D game with procedural shapes and coins)"}};
            } else {
                templateOptions = {
                    {TemplateChoice::ThreeD, "3D (procedural 3D world with lighting, camera, and physics)"},
                    {TemplateChoice::TwoD, "2D (pixel-space game with Renderer2D, shapes, and fonts)"},
                    {TemplateChoice::Headless, "Headless (deterministic simulation without window or GPU)"}};
            }

            int renderedLines = 0;
            while (true) {
                if (renderedLines > 0) {
                    moveCursorUp(renderedLines);
                }

                std::cout << "\r" << colors::bold() << "? Select template:" << colors::reset() << "\033[K\n";
                for (const auto& opt : templateOptions) {
                    std::cout << "\033[K";
                    if (model.answers().templateChoice == opt.first) {
                        std::cout << colors::cyan() << colors::bold() << "  ❯ " << opt.second
                                  << colors::reset() << "\n";
                    } else {
                        std::cout << colors::gray() << "    " << opt.second << colors::reset() << "\n";
                    }
                }
                renderedLines = 1 + static_cast<int>(templateOptions.size());
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Up || ch == 'k' || ch == 'K') {
                    model.selectPrevTemplate();
                } else if (key == Key::Down || ch == 'j' || ch == 'J') {
                    model.selectNextTemplate();
                } else if (key == Key::Enter) {
                    moveCursorUp(renderedLines);
                    for (int i = 0; i < renderedLines; ++i) {
                        clearCurrentLine();
                        if (i < renderedLines - 1) {
                            std::cout << "\n";
                        }
                    }
                    moveCursorUp(renderedLines - 1);
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " Template: " << colors::reset() << model.answers().templateString() << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Question 3: Audio (3D and 2D only)
        if (model.answers().templateChoice != TemplateChoice::Headless) {
            bool currentAudio = model.answers().audio;
            while (true) {
                clearCurrentLine();
                std::cout << colors::bold() << "? Enable synthesized audio module? " << colors::reset();
                if (currentAudio) {
                    std::cout << colors::cyan() << colors::bold() << "[Yes]" << colors::reset() << " No";
                } else {
                    std::cout << "Yes " << colors::cyan() << colors::bold() << "[No]" << colors::reset();
                }
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Left || key == Key::Right || key == Key::Tab || key == Key::Space) {
                    currentAudio = !currentAudio;
                    model.setAudio(currentAudio);
                } else if (ch == 'y' || ch == 'Y' || ch == 's' || ch == 'S') {
                    currentAudio = true;
                    model.setAudio(true);
                } else if (ch == 'n' || ch == 'N') {
                    currentAudio = false;
                    model.setAudio(false);
                } else if (key == Key::Enter) {
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " Audio module: " << colors::reset() << (currentAudio ? "Yes" : "No")
                              << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Question 4: Open-source license
        {
            const std::vector<std::pair<LicenseChoice, std::string>> licenseOptions = {
                {LicenseChoice::Mit, "MIT (permissive, standard)"},
                {LicenseChoice::Apache2, "Apache-2.0 (permissive with patent grant)"},
                {LicenseChoice::None, "None / Proprietary"}};

            int renderedLines = 0;
            while (true) {
                if (renderedLines > 0) {
                    moveCursorUp(renderedLines);
                }

                std::cout << "\r" << colors::bold() << "? Open-source license:" << colors::reset()
                          << "\033[K\n";
                for (const auto& opt : licenseOptions) {
                    std::cout << "\033[K";
                    if (model.answers().license == opt.first) {
                        std::cout << colors::cyan() << colors::bold() << "  ❯ " << opt.second
                                  << colors::reset() << "\n";
                    } else {
                        std::cout << colors::gray() << "    " << opt.second << colors::reset() << "\n";
                    }
                }
                renderedLines = 1 + static_cast<int>(licenseOptions.size());
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Up || ch == 'k' || ch == 'K') {
                    model.selectPrevLicense();
                } else if (key == Key::Down || ch == 'j' || ch == 'J') {
                    model.selectNextLicense();
                } else if (key == Key::Enter) {
                    moveCursorUp(renderedLines);
                    for (int i = 0; i < renderedLines; ++i) {
                        clearCurrentLine();
                        if (i < renderedLines - 1) {
                            std::cout << "\n";
                        }
                    }
                    moveCursorUp(renderedLines - 1);
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " License: " << colors::reset() << model.answers().licenseString() << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Question 5: Initialize Git
        {
            bool currentGit = model.answers().git;
            while (true) {
                clearCurrentLine();
                std::cout << colors::bold() << "? Initialize Git repository? " << colors::reset();
                if (currentGit) {
                    std::cout << colors::cyan() << colors::bold() << "[Yes]" << colors::reset() << " No";
                } else {
                    std::cout << "Yes " << colors::cyan() << colors::bold() << "[No]" << colors::reset();
                }
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Left || key == Key::Right || key == Key::Tab || key == Key::Space) {
                    currentGit = !currentGit;
                    model.setGit(currentGit);
                } else if (ch == 'y' || ch == 'Y' || ch == 's' || ch == 'S') {
                    currentGit = true;
                    model.setGit(true);
                } else if (ch == 'n' || ch == 'N') {
                    currentGit = false;
                    model.setGit(false);
                } else if (key == Key::Enter) {
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " Git repository: " << colors::reset() << (currentGit ? "Yes" : "No")
                              << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Question 6: VS Code tasks
        {
            bool currentVsCode = model.answers().vscode;
            while (true) {
                clearCurrentLine();
                std::cout << colors::bold() << "? Generate VS Code configuration (.vscode/)? "
                          << colors::reset();
                if (currentVsCode) {
                    std::cout << colors::cyan() << colors::bold() << "[Yes]" << colors::reset() << " No";
                } else {
                    std::cout << "Yes " << colors::cyan() << colors::bold() << "[No]" << colors::reset();
                }
                std::cout << std::flush;

                char ch = 0;
                Key key = readKey(customKeyReader, ch);
                if (key == Key::CtrlC || key == Key::Eof) {
                    guard.restore();
                    std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                    model.cancel();
                    return false;
                }
                if (key == Key::Left || key == Key::Right || key == Key::Tab || key == Key::Space) {
                    currentVsCode = !currentVsCode;
                    model.setVsCode(currentVsCode);
                } else if (ch == 'y' || ch == 'Y' || ch == 's' || ch == 'S') {
                    currentVsCode = true;
                    model.setVsCode(true);
                } else if (ch == 'n' || ch == 'N') {
                    currentVsCode = false;
                    model.setVsCode(false);
                } else if (key == Key::Enter) {
                    clearCurrentLine();
                    std::cout << colors::green() << "✓" << colors::reset() << colors::bold()
                              << " VS Code tasks: " << colors::reset() << (currentVsCode ? "Yes" : "No")
                              << "\n";
                    model.nextStep();
                    break;
                }
            }
        }

        // Summary and Confirmation screen
        std::cout << "\n" << colors::bold() << "Project summary:" << colors::reset() << "\n";
        std::cout << "  Name:        " << colors::cyan() << model.answers().name << colors::reset() << "\n";
        std::cout << "  Language:    " << model.answers().languageString() << "\n";
        std::cout << "  Template:    " << model.answers().templateString() << "\n";
        std::cout << "  Audio:       " << (model.answers().audio ? "Yes" : "No") << "\n";
        std::cout << "  License:     " << model.answers().licenseString() << "\n";
        std::cout << "  Git init:    " << (model.answers().git ? "Yes" : "No") << "\n";
        std::cout << "  VS Code:     " << (model.answers().vscode ? "Yes" : "No") << "\n";

        std::filesystem::path dest = model.answers().targetDir;
        if (dest.empty()) {
            dest = std::filesystem::current_path() / model.answers().name;
        }
        std::cout << "  Target:      " << dest.string() << "\n\n";

        bool confirmCreate = true;
        while (true) {
            clearCurrentLine();
            std::cout << colors::bold() << "Create project? " << colors::reset();
            if (confirmCreate) {
                std::cout << colors::green() << colors::bold() << "[Yes]" << colors::reset() << " No";
            } else {
                std::cout << "Yes " << colors::red() << colors::bold() << "[No]" << colors::reset();
            }
            std::cout << std::flush;

            char ch = 0;
            Key key = readKey(customKeyReader, ch);
            if (key == Key::CtrlC || key == Key::Eof) {
                guard.restore();
                std::cout << "\n" << colors::yellow() << "Cancelled." << colors::reset() << "\n";
                model.cancel();
                return false;
            }
            if (key == Key::Left || key == Key::Right || key == Key::Tab || key == Key::Space) {
                confirmCreate = !confirmCreate;
            } else if (ch == 'y' || ch == 'Y' || ch == 's' || ch == 'S') {
                confirmCreate = true;
            } else if (ch == 'n' || ch == 'N') {
                confirmCreate = false;
            } else if (key == Key::Enter) {
                clearCurrentLine();
                guard.restore();
                if (confirmCreate) {
                    model.nextStep();
                    return true;
                }
                std::cout << colors::yellow() << "Creation cancelled." << colors::reset() << "\n";
                model.cancel();
                return false;
            }
        }
    }

} // namespace mge::cli::wizard
