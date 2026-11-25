#pragma once

#include <mge/core/Input.h>

#include <SDL3/SDL.h>
#include <cstddef>

namespace mge {

    // SDL3 -> core input translation. Pure functions: testable without a window.
    Key keyFromScancode(SDL_Scancode scancode);
    // MouseButton::Count if the button is not mapped
    MouseButton mouseButtonFromSdl(Uint8 button);
    // GamepadButton::Count if the button is not mapped
    GamepadButton gamepadButtonFromSdl(Uint8 button);
    // GamepadAxis::Count if the axis is not mapped
    GamepadAxis gamepadAxisFromSdl(Uint8 axis);
    // Normalizes an SDL Sint16 axis value to [-1, 1] (sticks) or [0, 1] (triggers)
    float normalizeSdlGamepadAxis(Sint16 value, bool isTrigger);

    // Applies an SDL event to the input state (keys, mouse, gamepad, text input, focus)
    void applySdlEvent(InputState& input, const SDL_Event& event, std::size_t gamepadSlot = 0);
}
