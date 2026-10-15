#include <doctest/doctest.h>
#include <mge/platform/SdlInput.h>

using namespace mge;

// Eventos construidos a mano: no hace falta SDL_Init ni ventana

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

TEST_CASE("Traducción de scancodes") {
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

TEST_CASE("Botones del ratón") {
    CHECK(mouseButtonFromSdl(SDL_BUTTON_LEFT) == MouseButton::Left);
    CHECK(mouseButtonFromSdl(SDL_BUTTON_RIGHT) == MouseButton::Right);
    CHECK(mouseButtonFromSdl(SDL_BUTTON_X2) == MouseButton::X2);
    CHECK(mouseButtonFromSdl(99) == MouseButton::Count);
}

TEST_CASE("Eventos de teclado actualizan el estado; las repeticiones no generan flancos") {
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

TEST_CASE("Movimiento, botones y rueda del ratón") {
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

TEST_CASE("Perder el foco suelta todas las teclas") {
    InputState input;
    input.beginFrame();
    applySdlEvent(input, keyEvent(SDL_SCANCODE_LCTRL, true));
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    applySdlEvent(input, focus);
    CHECK_FALSE(input.isDown(Key::LeftCtrl));
}
