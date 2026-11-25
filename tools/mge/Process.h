#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mge::cli {

    // Runs a system command passing arguments as a list to avoid shell injection
    int runCommand(const std::vector<std::string>& args, const std::filesystem::path& workingDir = {});

    // Runs a command capturing combined standard output and error
    int runCommandCapture(const std::vector<std::string>& args, std::string& outOutput,
                          const std::filesystem::path& workingDir = {});

    // Checks whether a command exists in the system PATH
    bool commandExists(const std::string& commandName);

    // Returns the canonical absolute path of the running mge executable
    std::filesystem::path getExecutablePath();

} // namespace mge::cli
