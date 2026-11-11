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

    // Ejecuta las comprobaciones de dependencias del sistema y devuelve su estado
    std::vector<DoctorItem> runDoctorChecks();

    // Comando 'mge doctor' interactivo que formatea e imprime los resultados
    int cmdDoctor(int argc = 0, char* argv[] = nullptr);

} // namespace mge::cli
