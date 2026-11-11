#pragma once

#include <filesystem>

namespace mge::cli {

    int cmdNew(int argc, char* argv[], const std::filesystem::path& exePath);
    int cmdBuild(int argc, char* argv[]);
    int cmdRun(int argc, char* argv[]);
    int cmdTest(int argc, char* argv[]);
    int cmdCheck(int argc, char* argv[]);
    int cmdClean(int argc, char* argv[]);

    void printGeneralHelp();
    void printVersion();

} // namespace mge::cli
