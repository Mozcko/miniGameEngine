// NOTA DE DISEÑO:
// La regla del motor "no usar printf/std::cout/std::cerr" aplica a las bibliotecas del motor (mge_*).
// Una herramienta de línea de comandos (CLI) como mge DEBE comunicarse con el usuario a través de stdout
// y stderr para ser componible en tuberías y shells del sistema operativo.
// Por tanto, el uso de std::cout y std::cerr está explícitamente permitido en tools/mge.

#include "Commands.h"
#include "Doctor.h"
#include "Process.h"

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        mge::cli::printGeneralHelp();
        return 0;
    }

    std::string_view cmd = argv[1];
    if (cmd == "--help" || cmd == "-h") {
        mge::cli::printGeneralHelp();
        return 0;
    }
    if (cmd == "--version" || cmd == "-v") {
        mge::cli::printVersion();
        return 0;
    }

    auto exePath = mge::cli::getExecutablePath();

    if (cmd == "new") {
        return mge::cli::cmdNew(argc - 2, argv + 2, exePath);
    }
    if (cmd == "build") {
        return mge::cli::cmdBuild(argc - 2, argv + 2);
    }
    if (cmd == "run") {
        return mge::cli::cmdRun(argc - 2, argv + 2);
    }
    if (cmd == "test") {
        return mge::cli::cmdTest(argc - 2, argv + 2);
    }
    if (cmd == "check") {
        return mge::cli::cmdCheck(argc - 2, argv + 2);
    }
    if (cmd == "clean") {
        return mge::cli::cmdClean(argc - 2, argv + 2);
    }
    if (cmd == "doctor") {
        return mge::cli::cmdDoctor(argc - 2, argv + 2);
    }

    std::cerr << "Error: comando desconocido '" << cmd << "'.\n\n";
    mge::cli::printGeneralHelp();
    return 1;
}
