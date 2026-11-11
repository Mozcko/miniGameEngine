# 0005. Interactive Project Wizard (TUI) for `mge new`

- Status: Accepted
- Date: 2026-11-11
- Issues: #105

## Context
Running `mge new` should provide an intuitive, zero-friction developer experience for newcomers. When invoked without arguments in an interactive terminal, the CLI should launch a lightweight terminal user interface (TUI) wizard to guide the user through setting up a project name, template choice, modules (such as audio), license, git initialization, and editor configuration (VS Code).

We needed to decide how to implement this interactive wizard:
1. **Third-party TUI library** (such as FTXUI, ncurses, linenoise, or prompt-toolkit).
2. **Hand-rolled ANSI terminal interface** using POSIX `termios` raw mode and ANSI escape codes (with `SetConsoleMode` virtual terminal processing on Windows).

## Decision
We choose a **hand-rolled ANSI terminal approach** with a strict Model-View-Controller architecture:
- **No external dependencies**: The CLI tool continues to require only standard C++17 and the engine's `mge_core` library. Libraries like FTXUI add thousands of lines of code, complex layout trees, and increased binary size that conflict with the engine's lightweight design.
- **Pure model separation**: All business logic (questions, valid transitions, name validation, default answers, and file tree planning) resides in a headless `WizardModel` that is 100% unit-testable without a terminal.
- **Thin terminal view**: The view layer handles `termios` non-canonical/raw mode, parses arrow keys and control characters, and emits standard ANSI escape sequences (`\r`, `\033[K`, colors when enabled).
- **Graceful non-interactive fallback**: The wizard is **only** launched when both `stdin` and `stdout` are interactive TTYs (`isatty() != 0`). In non-interactive contexts (CI environments, pipes, background scripts, SSH without allocation), `mge new` never prompts: a missing name is reported immediately as an error with command usage, and all options can be specified via CLI flags (`--template`, `--no-audio`, `--license`, `--no-git`, `--vscode/--no-vscode`, `--yes`).
- **Terminal state safety**: Terminal settings are managed by an RAII guard (`TerminalGuard`) and signal handlers. If the user presses `Ctrl+C` at any point, the terminal attributes are restored immediately, no files are created, and the process exits cleanly.
- **Accessibility & terminal constraints**: Operates cleanly on standard 80×24 terminals and over SSH connections. Honors `NO_COLOR` and `TERM=dumb` by stripping all color escape codes.

## Alternatives considered
- **FTXUI / ncurses**: Provide rich widget layouts but introduce substantial external dependencies, CMake FetchContent overhead, and platform-specific linking complexity.
- **Line-by-line std::cin prompts (cin >> name)**: Simplistic and requires hitting Enter after every choice, but fails to provide a polished developer experience with arrow-key navigation and live name validation.

## Consequences
- The binary footprint of `mge` remains minimal (<500 KB).
- Automated CI scripts and pipelines are completely unaffected because non-TTY invocations bypass the wizard.
- The wizard model is verified via automated snapshot tests comparing planned file trees and content hashes against golden files.
