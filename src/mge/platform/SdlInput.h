#pragma once

#include <mge/core/Input.h>

#include <SDL3/SDL.h>

namespace mge {

    // Traducción SDL3 -> input del core. Funciones puras: se pueden testear sin ventana.
    Key keyFromScancode(SDL_Scancode scancode);
    // MouseButton::Count si el botón no está mapeado
    MouseButton mouseButtonFromSdl(Uint8 button);

    // Aplica un evento de SDL al estado de input (teclas, botones, movimiento, rueda, foco)
    void applySdlEvent(InputState& input, const SDL_Event& event);
}
