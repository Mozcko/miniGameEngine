#include <doctest/doctest.h>
#include <mge/core/Tween.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>

using namespace mge;

TEST_CASE("applyEase cumple los extremos 0 y 1 en todas las curvas y recorta fuera de rango") {
    constexpr std::array<Ease, 22> kAllEases = {
        Ease::Linear,    Ease::InQuad,     Ease::OutQuad,    Ease::InOutQuad, Ease::InCubic,
        Ease::OutCubic,  Ease::InOutCubic, Ease::InSine,     Ease::OutSine,   Ease::InOutSine,
        Ease::InExpo,    Ease::OutExpo,    Ease::InOutExpo,  Ease::InBack,    Ease::OutBack,
        Ease::InOutBack, Ease::InBounce,   Ease::OutBounce,  Ease::InOutBounce,
        Ease::InElastic, Ease::OutElastic, Ease::InOutElastic,
    };

    for (Ease ease : kAllEases) {
        CHECK(applyEase(ease, 0.0f) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(applyEase(ease, 1.0f) == doctest::Approx(1.0f).epsilon(1e-5));
        CHECK(applyEase(ease, -0.5f) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(applyEase(ease, 1.5f) == doctest::Approx(1.0f).epsilon(1e-5));
    }

    CHECK(applyEase(Ease::Linear, 0.5f) == doctest::Approx(0.5f));
    CHECK(applyEase(Ease::InQuad, 0.5f) == doctest::Approx(0.25f));
    CHECK(applyEase(Ease::OutQuad, 0.5f) == doctest::Approx(0.75f));
    CHECK(applyEase(Ease::InOutQuad, 0.5f) == doctest::Approx(0.5f));
    CHECK(applyEase(Ease::InCubic, 0.5f) == doctest::Approx(0.125f));
}

TEST_CASE("TweenManager: interpolación de float, glm::vec3 y glm::quat con onComplete") {
    TweenManager tweens;
    float scalar = 0.0f;
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    int completed = 0;

    const TweenHandle hScalar = tweens.to(scalar, 10.0f, 20.0f, 1.0f, Ease::Linear);
    tweens.setOnComplete(hScalar, [&]() { completed++; });

    const TweenHandle hPos =
        tweens.value<glm::vec3>({0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 6.0f}, 1.0f,
                                [&](const glm::vec3& v) { position = v; }, Ease::InQuad);

    const glm::quat targetRot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const TweenHandle hRot = tweens.to(rotation, rotation, targetRot, 1.0f, Ease::Linear);

    CHECK(tweens.activeCount() == 3);

    tweens.update(0.5f);
    CHECK(scalar == doctest::Approx(15.0f));
    CHECK(position.x == doctest::Approx(0.5f));
    CHECK(position.y == doctest::Approx(1.0f));
    CHECK(position.z == doctest::Approx(1.5f));
    CHECK(tweens.progress(hScalar) == doctest::Approx(0.5f));
    CHECK(completed == 0);

    tweens.update(0.5f);
    CHECK(scalar == doctest::Approx(20.0f));
    CHECK(position.x == doctest::Approx(2.0f));
    CHECK(position.y == doctest::Approx(4.0f));
    CHECK(position.z == doctest::Approx(6.0f));
    CHECK(glm::abs(glm::dot(rotation, targetRot)) == doctest::Approx(1.0f));
    CHECK(completed == 1);
    CHECK_FALSE(tweens.isActive(hScalar));
    CHECK_FALSE(tweens.isActive(hPos));
    CHECK_FALSE(tweens.isActive(hRot));
    CHECK(tweens.activeCount() == 0);
}

TEST_CASE("TweenManager: delay, pause, resume, cancel y duración cero") {
    TweenManager tweens;
    float value = 0.0f;
    int doneCount = 0;

    const TweenHandle h = tweens.to(value, 0.0f, 100.0f, 1.0f);
    tweens.setDelay(h, 0.5f);
    tweens.setOnComplete(h, [&]() { doneCount++; });

    tweens.update(0.3f);
    CHECK(value == doctest::Approx(0.0f));

    // Pasa 0.2 s de delay restante + 0.5 s de progreso real
    tweens.update(0.7f);
    CHECK(value == doctest::Approx(50.0f));

    tweens.pause(h);
    CHECK(tweens.isPaused(h));
    tweens.update(2.0f);
    CHECK(value == doctest::Approx(50.0f));

    tweens.resume(h);
    tweens.cancel(h);
    CHECK_FALSE(tweens.isActive(h));
    tweens.update(2.0f);
    CHECK(doneCount == 0);

    // Duración 0 completa de inmediato en el próximo update
    float instant = 0.0f;
    const TweenHandle zero = tweens.to(instant, 0.0f, 42.0f, 0.0f);
    tweens.setOnComplete(zero, [&]() { doneCount++; });
    tweens.update(0.016f);
    CHECK(instant == doctest::Approx(42.0f));
    CHECK(doneCount == 1);
    CHECK_FALSE(tweens.isActive(zero));
}

TEST_CASE("TweenManager: LoopMode Restart y PingPong con límite de ciclos") {
    TweenManager tweens;
    float restartVal = 0.0f;
    float pingPongVal = 0.0f;
    int restartDone = 0;
    int pingPongDone = 0;

    const TweenHandle hRestart = tweens.to(restartVal, 0.0f, 10.0f, 1.0f);
    tweens.setLoop(hRestart, LoopMode::Restart, 2);
    tweens.setOnComplete(hRestart, [&]() { restartDone++; });

    const TweenHandle hPing = tweens.to(pingPongVal, 0.0f, 10.0f, 1.0f);
    tweens.setLoop(hPing, LoopMode::PingPong, 2);
    tweens.setOnComplete(hPing, [&]() { pingPongDone++; });

    // 1.25 s: Restart está en 25% del segundo ciclo; PingPong está volviendo en 75%
    tweens.update(1.25f);
    CHECK(restartVal == doctest::Approx(2.5f));
    CHECK(pingPongVal == doctest::Approx(7.5f));
    CHECK(restartDone == 0);
    CHECK(pingPongDone == 0);

    // Al llegar a 2.0 s ambos completan: Restart acaba en 10 y PingPong (2 tramos) vuelve a 0
    tweens.update(0.75f);
    CHECK(restartVal == doctest::Approx(10.0f));
    CHECK(pingPongVal == doctest::Approx(0.0f));
    CHECK(restartDone == 1);
    CHECK(pingPongDone == 1);
    CHECK(tweens.activeCount() == 0);
}
