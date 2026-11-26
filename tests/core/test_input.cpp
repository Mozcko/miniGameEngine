#include <doctest/doctest.h>
#include <mge/core/Input.h>

using namespace mge;

TEST_CASE("Key edges: pressed only on first frame, held after, released on release") {
    InputState input;

    input.beginFrame();
    input.setKey(Key::W, true);
    CHECK(input.isDown(Key::W));
    CHECK(input.wasPressed(Key::W));
    CHECK_FALSE(input.wasReleased(Key::W));

    input.beginFrame(); // still held
    CHECK(input.isDown(Key::W));
    CHECK_FALSE(input.wasPressed(Key::W));

    input.beginFrame();
    input.setKey(Key::W, false);
    CHECK_FALSE(input.isDown(Key::W));
    CHECK(input.wasReleased(Key::W));

    input.beginFrame();
    CHECK_FALSE(input.wasReleased(Key::W));
}

TEST_CASE("Pressing and releasing within the same frame leaves no edges") {
    InputState input;
    input.beginFrame();
    input.setKey(Key::P, true);
    input.setKey(Key::P, false);
    CHECK_FALSE(input.isDown(Key::P));
    CHECK_FALSE(input.wasPressed(Key::P));
}

TEST_CASE("Mouse buttons have the same edges") {
    InputState input;
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, true);
    CHECK(input.wasPressed(MouseButton::Right));
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, false);
    CHECK(input.wasReleased(MouseButton::Right));
    CHECK_FALSE(input.isDown(MouseButton::Left));
}

TEST_CASE("Mouse delta and wheel accumulate within the frame and reset in beginFrame") {
    InputState input;
    input.beginFrame();
    input.addMouseDelta(3.0f, -1.0f);
    input.addMouseDelta(2.0f, 4.0f);
    input.addWheel(1.0f);
    input.setMousePosition(100.0f, 50.0f);
    CHECK(input.mouseDelta().x == doctest::Approx(5.0f));
    CHECK(input.mouseDelta().y == doctest::Approx(3.0f));
    CHECK(input.wheel() == doctest::Approx(1.0f));

    input.beginFrame();
    CHECK(input.mouseDelta().x == doctest::Approx(0.0f));
    CHECK(input.wheel() == doctest::Approx(0.0f));
    CHECK(input.mousePosition().x == doctest::Approx(100.0f)); // Position is preserved
}

TEST_CASE("clear releases everything (focus loss)") {
    InputState input;
    input.beginFrame();
    input.setKey(Key::A, true);
    input.setMouseButton(MouseButton::Left, true);
    input.clear();
    CHECK_FALSE(input.isDown(Key::A));
    CHECK_FALSE(input.isDown(MouseButton::Left));
}

TEST_CASE("Unknown and Count are ignored safely") {
    InputState input;
    input.setKey(Key::Unknown, true);
    input.setKey(Key::Count, true);
    input.setMouseButton(MouseButton::Count, true);
    CHECK_FALSE(input.isDown(Key::Unknown));
}

TEST_CASE("ActionMap: an action with multiple bindings") {
    InputState input;
    ActionMap actions;
    actions.bind("Jump", Key::Space);
    actions.bind("Jump", MouseButton::Right);

    input.beginFrame();
    CHECK_FALSE(actions.isDown(input, "Jump"));
    input.setMouseButton(MouseButton::Right, true);
    CHECK(actions.isDown(input, "Jump"));
    CHECK(actions.wasPressed(input, "Jump"));

    // With button still held, pressing Space is not a new "pressed" edge
    input.beginFrame();
    input.setKey(Key::Space, true);
    CHECK_FALSE(actions.wasPressed(input, "Jump"));

    // Releasing only one: action stays down
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, false);
    CHECK(actions.isDown(input, "Jump"));
    CHECK_FALSE(actions.wasReleased(input, "Jump"));

    // Releasing the last one: released
    input.beginFrame();
    input.setKey(Key::Space, false);
    CHECK(actions.wasReleased(input, "Jump"));

    CHECK_FALSE(actions.isDown(input, "NonExistent"));
}

TEST_CASE("ActionMap: axes sum +1/-1 and cancel out") {
    InputState input;
    ActionMap actions;
    actions.bindAxis("MoveForward", Key::W, Key::S);
    actions.bindAxis("MoveForward", Key::Up, Key::Down); // Arrow keys too

    input.beginFrame();
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(0.0f));
    input.setKey(Key::W, true);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(1.0f));
    input.setKey(Key::S, true);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(0.0f));
    input.setKey(Key::W, false);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(-1.0f));
    input.setKey(Key::Down, true); // -2 clamps to -1
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(-1.0f));
    CHECK(actions.axis(input, "NonExistent") == doctest::Approx(0.0f));
}

TEST_CASE("Gamepad: connection, buttons, edges, and disconnect") {
    InputState input;
    CHECK_FALSE(input.isGamepadConnected(0));
    CHECK(input.connectedGamepadCount() == 0);
    CHECK(input.gamepadName(0).empty());

    input.setGamepadConnected(0, true, "Test Controller");
    input.setGamepadConnected(2, true, "Player 3");
    CHECK(input.isGamepadConnected(0));
    CHECK_FALSE(input.isGamepadConnected(1));
    CHECK(input.isGamepadConnected(2));
    CHECK(input.connectedGamepadCount() == 2);
    CHECK(input.gamepadName(0) == "Test Controller");
    CHECK(input.gamepadName(2) == "Player 3");

    input.beginFrame();
    input.setGamepadButton(0, GamepadButton::South, true);
    CHECK(input.isDown(GamepadButton::South, 0));
    CHECK(input.wasPressed(GamepadButton::South, 0));
    CHECK_FALSE(input.wasReleased(GamepadButton::South, 0));
    CHECK_FALSE(input.isDown(GamepadButton::South, 2));

    input.beginFrame();
    CHECK(input.isDown(GamepadButton::South, 0));
    CHECK_FALSE(input.wasPressed(GamepadButton::South, 0));

    // Disconnecting clears held buttons and triggers wasReleased on that frame
    input.beginFrame();
    input.setGamepadConnected(0, false);
    CHECK_FALSE(input.isGamepadConnected(0));
    CHECK(input.connectedGamepadCount() == 1);
    CHECK(input.gamepadName(0).empty());
    CHECK_FALSE(input.isDown(GamepadButton::South, 0));
    CHECK(input.wasReleased(GamepadButton::South, 0));

    // Out-of-range slots and Count are safely ignored
    input.setGamepadConnected(InputState::kMaxGamepads, true, "Invalid");
    input.setGamepadButton(InputState::kMaxGamepads, GamepadButton::South, true);
    input.setGamepadButton(0, GamepadButton::Count, true);
    CHECK_FALSE(input.isGamepadConnected(InputState::kMaxGamepads));
    CHECK_FALSE(input.isDown(GamepadButton::South, InputState::kMaxGamepads));
    CHECK_FALSE(input.isDown(GamepadButton::Count, 0));
}

TEST_CASE("Gamepad: raw sticks, triggers, and clear") {
    InputState input;
    input.setGamepadConnected(0, true, "Pad");

    input.setGamepadAxis(0, GamepadAxis::LeftX, 0.10f);
    CHECK(input.gamepadAxisRaw(GamepadAxis::LeftX, 0) == doctest::Approx(0.10f));
    CHECK(input.gamepadAxis(GamepadAxis::LeftX, 0) == doctest::Approx(0.10f));

    input.setGamepadAxis(0, GamepadAxis::LeftX, 0.60f);
    input.setGamepadAxis(0, GamepadAxis::LeftY, -0.40f);
    CHECK(input.gamepadAxis(GamepadAxis::LeftX, 0) == doctest::Approx(0.60f));
    CHECK(input.gamepadStick(true, 0).x == doctest::Approx(0.60f));
    CHECK(input.gamepadStick(true, 0).y == doctest::Approx(-0.40f));

    // Triggers clamp to [0, 1]
    input.setGamepadAxis(0, GamepadAxis::RightTrigger, -0.5f);
    CHECK(input.gamepadAxisRaw(GamepadAxis::RightTrigger, 0) == doctest::Approx(0.0f));
    input.setGamepadAxis(0, GamepadAxis::RightTrigger, 1.5f);
    CHECK(input.gamepadAxisRaw(GamepadAxis::RightTrigger, 0) == doctest::Approx(1.0f));

    // clear() zeroes buttons and axes but preserves connection state
    input.setGamepadButton(0, GamepadButton::North, true);
    input.clear();
    CHECK(input.isGamepadConnected(0));
    CHECK_FALSE(input.isDown(GamepadButton::North, 0));
    CHECK(input.gamepadAxisRaw(GamepadAxis::LeftX, 0) == doctest::Approx(0.0f));
}

TEST_CASE("ActionMap: gamepad actions, analog axes, and rebinding") {
    InputState input;
    ActionMap actions;
    actions.bind("Jump", Key::Space);
    actions.bind("Jump", GamepadButton::South, 0);
    actions.bindAxis("MoveX", Key::D, Key::A);
    actions.bindAxis("MoveX", GamepadAxis::LeftX, 1.0f, 0);
    actions.bindAxis("MoveY", GamepadAxis::LeftY, -1.0f, 0);

    input.beginFrame();
    input.setGamepadButton(0, GamepadButton::South, true);
    CHECK(actions.isDown(input, "Jump"));
    CHECK(actions.wasPressed(input, "Jump"));

    input.setGamepadAxis(0, GamepadAxis::LeftX, 0.5f);
    input.setGamepadAxis(0, GamepadAxis::LeftY, -0.75f);
    CHECK(actions.axis(input, "MoveX") == doctest::Approx(0.5f));
    CHECK(actions.axis(input, "MoveY") == doctest::Approx(0.75f));

    // Rebind specific slot and replace all bindings
    actions.rebind("Jump", 0, InputBinding::fromKey(Key::W));
    const auto jumpBindings = actions.bindingsFor("Jump");
    REQUIRE(jumpBindings.size() == 2);
    CHECK(jumpBindings[0] == InputBinding::fromKey(Key::W));
    CHECK(jumpBindings[1] == InputBinding::fromGamepad(GamepadButton::South, 0));

    actions.unbind("Jump", InputBinding::fromKey(Key::W));
    CHECK(actions.bindingsFor("Jump").size() == 1);

    actions.rebind("Jump", InputBinding::fromMouse(MouseButton::Left));
    CHECK(actions.bindingsFor("Jump").size() == 1);
    CHECK(actions.bindingsFor("Jump")[0] == InputBinding::fromMouse(MouseButton::Left));

    actions.clearAction("Jump");
    CHECK(actions.bindingsFor("Jump").empty());
    actions.clearAxis("MoveY");
    CHECK(actions.axisBindingsFor("MoveY").empty());
    CHECK(actions.axisNames() == std::vector<std::string>{"MoveX"});
}

TEST_CASE("capturePressedInput detects key, mouse, and gamepad presses") {
    InputState input;
    input.beginFrame();
    CHECK_FALSE(capturePressedInput(input).has_value());

    input.setKey(Key::E, true);
    auto captured = capturePressedInput(input);
    REQUIRE(captured.has_value());
    CHECK(*captured == InputBinding::fromKey(Key::E));

    // Held key on next frame is not a fresh press
    input.beginFrame();
    CHECK_FALSE(capturePressedInput(input).has_value());

    input.setMouseButton(MouseButton::X1, true);
    captured = capturePressedInput(input);
    REQUIRE(captured.has_value());
    CHECK(*captured == InputBinding::fromMouse(MouseButton::X1));

    input.beginFrame();
    input.setGamepadButton(1, GamepadButton::Start, true);
    captured = capturePressedInput(input);
    REQUIRE(captured.has_value());
    CHECK(*captured == InputBinding::fromGamepad(GamepadButton::Start, 1));
}

TEST_CASE("ActionMap: binding string conversion and serialization round-trip") {
    CHECK(keyFromName(keyName(Key::LeftShift)) == Key::LeftShift);
    CHECK(mouseButtonFromName(mouseButtonName(MouseButton::Right)) == MouseButton::Right);
    CHECK(gamepadButtonFromName(gamepadButtonName(GamepadButton::DpadUp)) == GamepadButton::DpadUp);
    CHECK(gamepadAxisFromName(gamepadAxisName(GamepadAxis::RightTrigger)) == GamepadAxis::RightTrigger);

    const auto padDefault = bindingFromString("Gamepad:South");
    REQUIRE(padDefault.has_value());
    CHECK(*padDefault == InputBinding::fromGamepad(GamepadButton::South, 0));
    CHECK_FALSE(bindingFromString("Invalid:Foo").has_value());

    ActionMap original;
    original.bind("Attack", MouseButton::Left);
    original.bind("Attack", GamepadButton::West, 0);
    original.bind("Jump", Key::Space);
    original.bindAxis("MoveForward", Key::W, Key::S);
    original.bindAxis("MoveForward", GamepadAxis::LeftY, -1.0f, 0);

    const std::string saved = original.serialize();

    ActionMap loaded;
    CHECK(loaded.deserialize("# User bindings\n\n" + saved));
    CHECK(loaded.actionNames() == original.actionNames());
    CHECK(loaded.axisNames() == original.axisNames());
    CHECK(loaded.bindingsFor("Attack") == original.bindingsFor("Attack"));
    CHECK(loaded.bindingsFor("Jump") == original.bindingsFor("Jump"));
    CHECK(loaded.axisBindingsFor("MoveForward") == original.axisBindingsFor("MoveForward"));

    // Malformed input fails and preserves existing bindings
    CHECK_FALSE(loaded.deserialize("action Broken = Key:NotARealKey\n"));
    CHECK(loaded.bindingsFor("Jump") == original.bindingsFor("Jump"));
}

TEST_CASE("Text input accumulates per frame and IME composition persists until cleared") {
    InputState input;
    input.beginFrame();
    input.addTextInput("Hello ");
    input.addTextInput("caf\xC3\xA9");
    CHECK(input.textInput() == "Hello caf\xC3\xA9");

    input.setTextComposition("\xE6\x97\xA5\xE6\x9C\xAC", 1, 1);
    CHECK(input.textComposition().active());
    CHECK(input.textComposition().text == "\xE6\x97\xA5\xE6\x9C\xAC");
    CHECK(input.textComposition().cursor == 1);
    CHECK(input.textComposition().selectionLength == 1);

    // beginFrame clears committed text input but preserves ongoing IME composition
    input.beginFrame();
    CHECK(input.textInput().empty());
    CHECK(input.textComposition().active());

    input.clearTextComposition();
    CHECK_FALSE(input.textComposition().active());

    input.setTextComposition("draft", 0, 5);
    input.addTextInput("abc");
    input.clear();
    CHECK(input.textInput().empty());
    CHECK_FALSE(input.textComposition().active());
}
