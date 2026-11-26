#pragma once

#include "Process.h"

#include <cstdlib>
#include <string_view>

namespace mge::cli::colors {

    inline bool areEnabled() {
        // Disabled if NO_COLOR is set in the environment
        const char* noColor = std::getenv("NO_COLOR");
        if (noColor != nullptr && noColor[0] != '\0') {
            return false;
        }

        // Disabled if the terminal type is 'dumb'
        const char* term = std::getenv("TERM");
        if (term != nullptr && std::string_view(term) == "dumb") {
            return false;
        }

        // Only enable if stdout is actually an interactive terminal (TTY) with ANSI support
        return isStdoutTty() && enableTerminalColors();
    }

    inline const char* reset() {
        return areEnabled() ? "\033[0m" : "";
    }
    inline const char* bold() {
        return areEnabled() ? "\033[1m" : "";
    }
    inline const char* red() {
        return areEnabled() ? "\033[31m" : "";
    }
    inline const char* green() {
        return areEnabled() ? "\033[32m" : "";
    }
    inline const char* yellow() {
        return areEnabled() ? "\033[33m" : "";
    }
    inline const char* blue() {
        return areEnabled() ? "\033[34m" : "";
    }
    inline const char* cyan() {
        return areEnabled() ? "\033[36m" : "";
    }
    inline const char* gray() {
        return areEnabled() ? "\033[90m" : "";
    }

} // namespace mge::cli::colors
