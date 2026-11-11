#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mge::cli {

    // Ejecuta un comando del sistema pasando argumentos como lista para evitar inyecciones de shell
    int runCommand(const std::vector<std::string>& args, const std::filesystem::path& workingDir = {});

    // Ejecuta un comando capturando la salida estándar combinada
    int runCommandCapture(const std::vector<std::string>& args, std::string& outOutput,
                          const std::filesystem::path& workingDir = {});

    // Comprueba si un comando existe en el PATH del sistema
    bool commandExists(const std::string& commandName);

    // Obtiene la ruta canónica absoluta del ejecutable mge en ejecución
    std::filesystem::path getExecutablePath();

} // namespace mge::cli
