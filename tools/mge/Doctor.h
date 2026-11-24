#pragma once

#include <string>
#include <vector>

namespace mge::cli {

    struct DoctorItem {
        std::string name;
        bool required{true};
        bool ok{false};
        std::string detail;
        std::string hint;
    };

    // Runs system dependency checks and returns their status
    std::vector<DoctorItem> runDoctorChecks();

    // Runs dependency checks for Python projects
    std::vector<DoctorItem> runPythonDoctorChecks();

    // Interactive 'mge doctor' command that formats and prints results
    int cmdDoctor(int argc = 0, char* argv[] = nullptr);

} // namespace mge::cli
