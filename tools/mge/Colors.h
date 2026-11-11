#pragma once

#include <cstdlib>
#include <string_view>
#include <unistd.h>

namespace mge::cli::colors {

    inline bool areEnabled() {
        // Desactivado si NO_COLOR está definido en el entorno
        const char* noColor = std::getenv("NO_COLOR");
        if (noColor != nullptr && noColor[0] != '\0') {
            return false;
        }

        // Desactivado si la terminal es de tipo 'dumb'
        const char* term = std::getenv("TERM");
        if (term != nullptr && std::string_view(term) == "dumb") {
            return false;
        }

        // Solo activar si stdout es efectivamente una terminal interactiva (TTY)
        return isatty(STDOUT_FILENO) != 0;
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
