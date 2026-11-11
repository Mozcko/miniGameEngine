#pragma once

#include "WizardModel.h"

#include <functional>

namespace mge::cli::wizard {

    // Tipos de teclas interpretadas por el asistente de proyectos
    enum class Key {
        None,
        Enter,
        Backspace,
        Up,
        Down,
        Left,
        Right,
        Space,
        Tab,
        CtrlC,
        Char,
        Eof,
        Escape
    };

    using KeyReader = std::function<Key(char&)>;

    // Ejecuta el asistente interactivo de proyectos en la terminal
    // Devuelve true si el usuario confirmó la creación, o false si canceló con Ctrl+C, EOF o 'n'
    bool runInteractiveWizard(WizardModel& model, const KeyReader& customKeyReader = {});

} // namespace mge::cli::wizard
