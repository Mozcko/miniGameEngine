#include <doctest/doctest.h>
#include <mge/platform/SdlInput.h>

using namespace mge;

// Hand-crafted events: no SDL_Init or window required

namespace {
    SDL_Event keyEvent(SDL_Scancode scancode, bool down, bool repeat = false) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = repeat;
        return event;
    }
}

TEST_CASE("Scancode translation") {
    CHECK(keyFromScancode(SDL_SCANCODE_A) == Key::A);
    CHECK(keyFromScancode(SDL_SCANCODE_Z) == Key::Z);
    CHECK(keyFromScancode(SDL_SCANCODE_1) == Key::Num1);
    CHECK(keyFromScancode(SDL_SCANCODE_9) == Key::Num9);
    CHECK(keyFromScancode(SDL_SCANCODE_0) == Key::Num0);
    CHECK(keyFromScancode(SDL_SCANCODE_F1) == Key::F1);
    CHECK(keyFromScancode(SDL_SCANCODE_F12) == Key::F12);
    CHECK(keyFromScancode(SDL_SCANCODE_LSHIFT) == Key::LeftShift);
    CHECK(keyFromScancode(SDL_SCANCODE_ESCAPE) == Key::Escape);
    CHECK(keyFromScancode(SDL_SCANCODE_KP_PLUS) == Key::Unknown);
}

TEST_CASE("Mouse buttons") {
    CHECK(mouseButtonFromSdl(SDL_BUTTON_LEFT) == MouseButton::Left);
    CHECK(mouseButtonFromSdl(SDL_BUTTON_RIGHT) == MouseButton::Right);
    CHECK(mouseButtonFromSdl(SDL_BUTTON_X2) == MouseButton::X2);
    CHECK(mouseButtonFromSdl(99) == MouseButton::Count);
}

TEST_CASE("Keyboard events update state; key repeats do not trigger edges") {
    InputState input;
    input.beginFrame();
    applySdlEvent(input, keyEvent(SDL_SCANCODE_W, true));
    CHECK(input.wasPressed(Key::W));

    input.beginFrame();
    applySdlEvent(input, keyEvent(SDL_SCANCODE_W, true, /*repeat=*/true));
    CHECK(input.isDown(Key::W));
    CHECK_FALSE(input.wasPressed(Key::W));

    input.beginFrame();
    applySdlEvent(input, keyEvent(SDL_SCANCODE_W, false));
    CHECK(input.wasReleased(Key::W));
}

TEST_CASE("Mouse motion, buttons, and wheel") {
    InputState input;
    input.beginFrame();

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 10.0f;
    motion.motion.y = 20.0f;
    motion.motion.xrel = 1.5f;
    motion.motion.yrel = -2.0f;
    applySdlEvent(input, motion);
    applySdlEvent(input, motion);
    CHECK(input.mousePosition().x == doctest::Approx(10.0f));
    CHECK(input.mouseDelta().x == doctest::Approx(3.0f));
    CHECK(input.mouseDelta().y == doctest::Approx(-4.0f));

    SDL_Event button{};
    button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button.button.button = SDL_BUTTON_LEFT;
    button.button.down = true;
    applySdlEvent(input, button);
    CHECK(input.wasPressed(MouseButton::Left));

    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 1.0f;
    applySdlEvent(input, wheel);
    CHECK(input.wheel() == doctest::Approx(1.0f));
}

TEST_CASE("Losing focus releases all keys") {
    InputState input;
    input.beginFrame();
    applySdlEvent(input, keyEvent(SDL_SCANCODE_LCTRL, true));
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    applySdlEvent(input, focus);
    CHECK_FALSE(input.isDown(Key::LeftCtrl));
}

TEST_CASE("Gamepad button and axis translation") {
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_SOUTH) == GamepadButton::South);
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_EAST) == GamepadButton::East);
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_WEST) == GamepadButton::West);
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_NORTH) == GamepadButton::North);
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) == GamepadButton::LeftShoulder);
    CHECK(gamepadButtonFromSdl(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) == GamepadButton::DpadRight);
    CHECK(gamepadButtonFromSdl(200) == GamepadButton::Count);

    CHECK(gamepadAxisFromSdl(SDL_GAMEPAD_AXIS_LEFTX) == GamepadAxis::LeftX);
    CHECK(gamepadAxisFromSdl(SDL_GAMEPAD_AXIS_RIGHTY) == GamepadAxis::RightY);
    CHECK(gamepadAxisFromSdl(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == GamepadAxis::LeftTrigger);
    CHECK(gamepadAxisFromSdl(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == GamepadAxis::RightTrigger);
    CHECK(gamepadAxisFromSdl(99) == GamepadAxis::Count);

    CHECK(normalizeSdlGamepadAxis(-32768, false) == doctest::Approx(-1.0f));
    CHECK(normalizeSdlGamepadAxis(0, false) == doctest::Approx(0.0f));
    CHECK(normalizeSdlGamepadAxis(32767, false) == doctest::Approx(1.0f));
    CHECK(normalizeSdlGamepadAxis(0, true) == doctest::Approx(0.0f));
    CHECK(normalizeSdlGamepadAxis(32767, true) == doctest::Approx(1.0f));
    CHECK(normalizeSdlGamepadAxis(-1000, true) == doctest::Approx(0.0f));
}

TEST_CASE("Gamepad SDL events update state for the target slot") {
    InputState input;
    input.setGamepadDeadzone(0.0f);
    input.beginFrame();

    SDL_Event btnDown{};
    btnDown.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    btnDown.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    btnDown.gbutton.down = true;
    applySdlEvent(input, btnDown, 1);
    CHECK(input.isDown(GamepadButton::South, 1));
    CHECK(input.wasPressed(GamepadButton::South, 1));
    CHECK_FALSE(input.isDown(GamepadButton::South, 0));

    SDL_Event axis{};
    axis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    axis.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axis.gaxis.value = -32768;
    applySdlEvent(input, axis, 1);
    CHECK(input.gamepadAxis(GamepadAxis::LeftX, 1) == doctest::Approx(-1.0f));

    SDL_Event trigger{};
    trigger.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    trigger.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    trigger.gaxis.value = 32767;
    applySdlEvent(input, trigger, 1);
    CHECK(input.gamepadAxis(GamepadAxis::RightTrigger, 1) == doctest::Approx(1.0f));

    input.beginFrame();
    SDL_Event btnUp{};
    btnUp.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    btnUp.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    btnUp.gbutton.down = false;
    applySdlEvent(input, btnUp, 1);
    CHECK(input.wasReleased(GamepadButton::South, 1));
}

TEST_CASE("Text input and IME editing SDL events") {
    InputState input;
    input.beginFrame();

    SDL_Event editing{};
    editing.type = SDL_EVENT_TEXT_EDITING;
    editing.edit.text = "ni";
    editing.edit.start = 0;
    editing.edit.length = 2;
    applySdlEvent(input, editing);
    CHECK(input.textComposition().active());
    CHECK(input.textComposition().text == "ni");
    CHECK(input.textComposition().cursor == 0);
    CHECK(input.textComposition().selectionLength == 2);

    SDL_Event committed{};
    committed.type = SDL_EVENT_TEXT_INPUT;
    committed.text.text = "Hello";
    applySdlEvent(input, committed);
    CHECK(input.textInput() == "Hello");
    CHECK_FALSE(input.textComposition().active());

    // Empty editing event clears composition
    applySdlEvent(input, editing);
    CHECK(input.textComposition().active());
    editing.edit.text = "";
    applySdlEvent(input, editing);
    CHECK_FALSE(input.textComposition().active());
}
