#pragma once

#include "WizardModel.h"

namespace mge::cli::wizard {

    // Ejecuta el asistente interactivo de proyectos en la terminal
    // Devuelve true si el usuario confirmó la creación, o false si canceló con Ctrl+C o 'n'
    bool runInteractiveWizard(WizardModel& model);

} // namespace mge::cli::wizard
