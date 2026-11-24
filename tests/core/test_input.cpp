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
