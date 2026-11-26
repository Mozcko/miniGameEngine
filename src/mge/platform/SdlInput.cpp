#include <mge/platform/SdlInput.h>

#include <algorithm>

namespace mge {

    Key keyFromScancode(SDL_Scancode scancode) {
        // Letters and digits are contiguous in both SDL and mge::Key
        if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
            return static_cast<Key>(static_cast<int>(Key::A) + (scancode - SDL_SCANCODE_A));
        }
        // In SDL the order is 1..9 followed by 0
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

    GamepadButton gamepadButtonFromSdl(Uint8 button) {
        switch (static_cast<SDL_GamepadButton>(button)) {
        case SDL_GAMEPAD_BUTTON_SOUTH: return GamepadButton::South;
        case SDL_GAMEPAD_BUTTON_EAST: return GamepadButton::East;
        case SDL_GAMEPAD_BUTTON_WEST: return GamepadButton::West;
        case SDL_GAMEPAD_BUTTON_NORTH: return GamepadButton::North;
        case SDL_GAMEPAD_BUTTON_BACK: return GamepadButton::Back;
        case SDL_GAMEPAD_BUTTON_GUIDE: return GamepadButton::Guide;
        case SDL_GAMEPAD_BUTTON_START: return GamepadButton::Start;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return GamepadButton::LeftStick;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return GamepadButton::RightStick;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return GamepadButton::LeftShoulder;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightShoulder;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return GamepadButton::DpadUp;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return GamepadButton::DpadDown;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return GamepadButton::DpadLeft;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButton::DpadRight;
        default: return GamepadButton::Count;
        }
    }

    GamepadAxis gamepadAxisFromSdl(Uint8 axis) {
        switch (static_cast<SDL_GamepadAxis>(axis)) {
        case SDL_GAMEPAD_AXIS_LEFTX: return GamepadAxis::LeftX;
        case SDL_GAMEPAD_AXIS_LEFTY: return GamepadAxis::LeftY;
        case SDL_GAMEPAD_AXIS_RIGHTX: return GamepadAxis::RightX;
        case SDL_GAMEPAD_AXIS_RIGHTY: return GamepadAxis::RightY;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return GamepadAxis::LeftTrigger;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
        default: return GamepadAxis::Count;
        }
    }

    float normalizeSdlGamepadAxis(Sint16 value, bool isTrigger) {
        if (isTrigger) {
            return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
        }
        const float normalized =
            value < 0 ? static_cast<float>(value) / 32768.0f : static_cast<float>(value) / 32767.0f;
        return std::clamp(normalized, -1.0f, 1.0f);
    }

    void applySdlEvent(InputState& input, const SDL_Event& event, std::size_t gamepadSlot) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            // Auto-repeats do not change state (the key is already down)
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
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            input.setGamepadButton(gamepadSlot, gamepadButtonFromSdl(event.gbutton.button),
                                   event.gbutton.down);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const GamepadAxis axis = gamepadAxisFromSdl(event.gaxis.axis);
            const bool isTrigger = (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger);
            input.setGamepadAxis(gamepadSlot, axis, normalizeSdlGamepadAxis(event.gaxis.value, isTrigger));
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (event.text.text != nullptr) {
                input.addTextInput(event.text.text);
                input.clearTextComposition();
            }
            break;
        case SDL_EVENT_TEXT_EDITING:
            if (event.edit.text != nullptr && event.edit.text[0] != '\0') {
                input.setTextComposition(event.edit.text, event.edit.start, event.edit.length);
            } else {
                input.clearTextComposition();
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            // Without focus, "key up" events are missed: release everything to avoid stuck keys
            input.clear();
            break;
        default: break;
        }
    }
}
