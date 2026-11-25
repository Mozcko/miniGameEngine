#pragma once

#include "WizardModel.h"

#include <functional>

namespace mge::cli::wizard {

    // Key types interpreted by the project wizard
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

    // Runs the interactive project wizard in the terminal
    // Returns true if the user confirmed creation, or false if cancelled with Ctrl+C, EOF, or 'n'
    bool runInteractiveWizard(WizardModel& model, const KeyReader& customKeyReader = {});

} // namespace mge::cli::wizard
