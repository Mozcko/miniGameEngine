#pragma once

#include <mge/core/Input.h>

#include <SDL3/SDL.h>

namespace mge {

    // SDL3 -> core input translation. Pure functions: testable without a window.
    Key keyFromScancode(SDL_Scancode scancode);
    // MouseButton::Count if the button is not mapped
    MouseButton mouseButtonFromSdl(Uint8 button);

    // Applies an SDL event to the input state (keys, buttons, motion, wheel, focus)
    void applySdlEvent(InputState& input, const SDL_Event& event);
}
