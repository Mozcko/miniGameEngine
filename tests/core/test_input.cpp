#include <doctest/doctest.h>
#include <mge/core/Input.h>

using namespace mge;

TEST_CASE("Flancos de tecla: pressed solo el primer frame, held después, released al soltar") {
    InputState input;

    input.beginFrame();
    input.setKey(Key::W, true);
    CHECK(input.isDown(Key::W));
    CHECK(input.wasPressed(Key::W));
    CHECK_FALSE(input.wasReleased(Key::W));

    input.beginFrame(); // sigue apretada
    CHECK(input.isDown(Key::W));
    CHECK_FALSE(input.wasPressed(Key::W));

    input.beginFrame();
    input.setKey(Key::W, false);
    CHECK_FALSE(input.isDown(Key::W));
    CHECK(input.wasReleased(Key::W));

    input.beginFrame();
    CHECK_FALSE(input.wasReleased(Key::W));
}

TEST_CASE("Apretar y soltar dentro del mismo frame no deja flancos") {
    InputState input;
    input.beginFrame();
    input.setKey(Key::P, true);
    input.setKey(Key::P, false);
    CHECK_FALSE(input.isDown(Key::P));
    CHECK_FALSE(input.wasPressed(Key::P));
}

TEST_CASE("Botones del ratón tienen los mismos flancos") {
    InputState input;
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, true);
    CHECK(input.wasPressed(MouseButton::Right));
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, false);
    CHECK(input.wasReleased(MouseButton::Right));
    CHECK_FALSE(input.isDown(MouseButton::Left));
}

TEST_CASE("Delta del ratón y rueda se acumulan en el frame y se reinician en beginFrame") {
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
    CHECK(input.mousePosition().x == doctest::Approx(100.0f)); // La posición se mantiene
}

TEST_CASE("clear suelta todo (pérdida de foco)") {
    InputState input;
    input.beginFrame();
    input.setKey(Key::A, true);
    input.setMouseButton(MouseButton::Left, true);
    input.clear();
    CHECK_FALSE(input.isDown(Key::A));
    CHECK_FALSE(input.isDown(MouseButton::Left));
}

TEST_CASE("Unknown y Count se ignoran sin romper nada") {
    InputState input;
    input.setKey(Key::Unknown, true);
    input.setKey(Key::Count, true);
    input.setMouseButton(MouseButton::Count, true);
    CHECK_FALSE(input.isDown(Key::Unknown));
}

TEST_CASE("ActionMap: una acción con varios bindings") {
    InputState input;
    ActionMap actions;
    actions.bind("Jump", Key::Space);
    actions.bind("Jump", MouseButton::Right);

    input.beginFrame();
    CHECK_FALSE(actions.isDown(input, "Jump"));
    input.setMouseButton(MouseButton::Right, true);
    CHECK(actions.isDown(input, "Jump"));
    CHECK(actions.wasPressed(input, "Jump"));

    // Con el botón todavía apretado, apretar Espacio no es un nuevo "pressed"
    input.beginFrame();
    input.setKey(Key::Space, true);
    CHECK_FALSE(actions.wasPressed(input, "Jump"));

    // Soltar solo uno: la acción sigue abajo
    input.beginFrame();
    input.setMouseButton(MouseButton::Right, false);
    CHECK(actions.isDown(input, "Jump"));
    CHECK_FALSE(actions.wasReleased(input, "Jump"));

    // Soltar el último: released
    input.beginFrame();
    input.setKey(Key::Space, false);
    CHECK(actions.wasReleased(input, "Jump"));

    CHECK_FALSE(actions.isDown(input, "NoExiste"));
}

TEST_CASE("ActionMap: ejes suman +1/-1 y se cancelan") {
    InputState input;
    ActionMap actions;
    actions.bindAxis("MoveForward", Key::W, Key::S);
    actions.bindAxis("MoveForward", Key::Up, Key::Down); // Flechas también

    input.beginFrame();
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(0.0f));
    input.setKey(Key::W, true);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(1.0f));
    input.setKey(Key::S, true);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(0.0f));
    input.setKey(Key::W, false);
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(-1.0f));
    input.setKey(Key::Down, true); // -2 se recorta a -1
    CHECK(actions.axis(input, "MoveForward") == doctest::Approx(-1.0f));
    CHECK(actions.axis(input, "NoExiste") == doctest::Approx(0.0f));
}
