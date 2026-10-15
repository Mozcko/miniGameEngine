#include <mge/platform/SdlInput.h>

namespace mge {

    Key keyFromScancode(SDL_Scancode scancode) {
        // Letras y números son contiguos tanto en SDL como en mge::Key
        if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
            return static_cast<Key>(static_cast<int>(Key::A) + (scancode - SDL_SCANCODE_A));
        }
        // En SDL el orden es 1..9 y luego 0
        if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
            return static_cast<Key>(static_cast<int>(Key::Num1) + (scancode - SDL_SCANCODE_1));
        }
        if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
            return static_cast<Key>(static_cast<int>(Key::F1) + (scancode - SDL_SCANCODE_F1));
        }
        switch (scancode) {
        case SDL_SCANCODE_0: return Key::Num0;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
        case SDL_SCANCODE_RSHIFT: return Key::RightShift;
        case SDL_SCANCODE_LCTRL: return Key::LeftCtrl;
        case SDL_SCANCODE_RCTRL: return Key::RightCtrl;
        case SDL_SCANCODE_LALT: return Key::LeftAlt;
        case SDL_SCANCODE_RALT: return Key::RightAlt;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        default: return Key::Unknown;
        }
    }

    MouseButton mouseButtonFromSdl(Uint8 button) {
        switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return MouseButton::Count;
        }
    }

    void applySdlEvent(InputState& input, const SDL_Event& event) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            // Las repeticiones automáticas no cambian el estado (la tecla ya está abajo)
            if (!event.key.repeat) input.setKey(keyFromScancode(event.key.scancode), event.key.down);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            input.setMouseButton(mouseButtonFromSdl(event.button.button), event.button.down);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.setMousePosition(event.motion.x, event.motion.y);
            input.addMouseDelta(event.motion.xrel, event.motion.yrel);
            break;
        case SDL_EVENT_MOUSE_WHEEL: input.addWheel(event.wheel.y); break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            // Sin foco no llegan los "key up": soltar todo para no quedar con teclas pegadas
            input.clear();
            break;
        default: break;
        }
    }
}
