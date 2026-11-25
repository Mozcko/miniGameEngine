// DESIGN NOTE:
// The engine rule "do not use printf/std::cout/std::cerr" applies to the engine libraries (mge_*).
// A command-line tool (CLI) like mge MUST communicate with the user through stdout and stderr
// so it can be composed in OS pipes and shells.
// Therefore, using std::cout and std::cerr is explicitly allowed in tools/mge.

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

    std::cerr << "Error: unknown command '" << cmd << "'.\n\n";
    mge::cli::printGeneralHelp();
    return 1;
}
