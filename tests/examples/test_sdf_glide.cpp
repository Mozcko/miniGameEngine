#include <doctest/doctest.h>
#include <examples/sdf_glide/SdfGlideLogic.h>
#include <mge/core/Random.h>
#include <mge/procgen/Sdf.h>
#include <mge/synth/Sfx.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace sdf_glide;

TEST_CASE("Primitivas SDF en C++ coinciden con mge::sdf") {
    // Esfera
    for (float r : {0.5f, 1.0f, 2.5f}) {
        auto mgeSphere = mge::sdf::sphere(r);
        for (const glm::vec3& p :
             {glm::vec3(0.0f), glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)}) {
            CHECK(sdSphere(p, r) == doctest::Approx(mgeSphere(p)));
        }
    }

    // Caja
    const glm::vec3 halfExtents(1.0f, 2.0f, 0.5f);
    auto mgeBox = mge::sdf::box(halfExtents);
    for (const glm::vec3& p : {glm::vec3(0.0f), glm::vec3(3.0f, 1.0f, 0.0f), glm::vec3(1.5f, 2.5f, 1.0f)}) {
        CHECK(sdBox(p, halfExtents) == doctest::Approx(mgeBox(p)));
    }

    // Toro
    const float majorRadius = 2.0f;
    const float minorRadius = 0.4f;
    auto mgeTorus = mge::sdf::torus(majorRadius, minorRadius);
    for (const glm::vec3& p :
         {glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.5f, 2.0f)}) {
        CHECK(sdTorus(p, majorRadius, minorRadius) == doctest::Approx(mgeTorus(p)));
    }

    // Unión suave
    const auto sphere1 = mge::sdf::sphere(1.0f);
    const auto sphere2 = mge::sdf::opTranslate(mge::sdf::sphere(1.0f), {1.2f, 0.0f, 0.0f});
    const auto smoothU = mge::sdf::opSmoothUnion(sphere1, sphere2, 0.5f);
    for (float x = -2.0f; x <= 3.0f; x += 0.5f) {
        const glm::vec3 p(x, 0.2f, 0.0f);
        const float d1 = sdSphere(p, 1.0f);
        const float d2 = sdSphere(p - glm::vec3(1.2f, 0.0f, 0.0f), 1.0f);
        CHECK(opSmoothUnion(d1, d2, 0.5f) == doctest::Approx(smoothU(p)));
    }

    // Twist
    const float radiansPerUnit = 0.3f;
    const auto twistedBox = mge::sdf::opTwist(mge::sdf::box({1.0f, 1.0f, 1.0f}), radiansPerUnit);
    for (float y = -2.0f; y <= 2.0f; y += 1.0f) {
        const glm::vec3 p(0.8f, y, 0.5f);
        const glm::vec3 tp = opTwist(p, radiansPerUnit);
        CHECK(sdBox(tp, {1.0f, 1.0f, 1.0f}) == doctest::Approx(twistedBox(p)));
    }
}

TEST_CASE("Muestreo de 1000 puntos en el cañón procedural (finitud y continuidad Lipschitz)") {
    mge::Pcg32 rng(1337);

    for (int i = 0; i < 1000; ++i) {
        const float x = rng.range(-12.0f, 12.0f);
        const float y = rng.range(-6.0f, 16.0f);
        const float z = rng.range(-500.0f, 500.0f);
        const glm::vec3 p(x, y, z);

        const float d = canyonDistance(p);
        CHECK_FALSE(std::isnan(d));
        CHECK_FALSE(std::isinf(d));

        // Gradiente / normal en p
        const glm::vec3 n = canyonNormal(p);
        CHECK_FALSE(std::isnan(n.x));
        CHECK_FALSE(std::isnan(n.y));
        CHECK_FALSE(std::isnan(n.z));

        // Comprobación de condición Lipschitz con un punto cercano
        const glm::vec3 delta(0.05f, -0.04f, 0.03f);
        const float dPerturbed = canyonDistance(p + delta);
        const float diff = std::abs(dPerturbed - d);
        const float maxExpected = glm::length(delta) * 1.5f; // Margen para opTwist y smooth union
        CHECK(diff <= maxExpected);
    }
}

TEST_CASE("sectionHash y sectionHashU32 son deterministas y bien distribuidos") {
    // Mismo input produce exactamente el mismo resultado
    CHECK(sectionHashU32(42, 10) == sectionHashU32(42, 10));
    CHECK(sectionHash(42, 10) == sectionHash(42, 10));

    // Rango [0, 1)
    for (int s = -10; s <= 50; ++s) {
        for (uint32_t salt = 0; salt < 5; ++salt) {
            const float h = sectionHash(s, salt);
            CHECK(h >= 0.0f);
            CHECK(h < 1.0f);
        }
    }

    // Secciones consecutivas difieren
    for (int s = 0; s < 10; ++s) {
        CHECK(sectionHashU32(s, 1) != sectionHashU32(s + 1, 1));
    }
}

TEST_CASE("GlideGame: inicialización y estado inicial") {
    GlideGame game;
    CHECK_FALSE(game.isGameOver());
    CHECK(game.shields() == 3);
    CHECK(game.energy() == doctest::Approx(100.0f));
    CHECK(game.score() == 0);
    CHECK(game.combo() == 1);
    CHECK(game.distanceTraveled() == doctest::Approx(0.0f));
    CHECK(game.playerPosition().x == doctest::Approx(0.0f));
    CHECK(game.playerPosition().y == doctest::Approx(0.0f));
    CHECK(game.playerPosition().z == doctest::Approx(0.0f));
}

TEST_CASE("GlideGame: avance hacia adelante y aceleración") {
    GlideGame game;
    GlideInput input;

    // Avanzar 1 segundo
    for (int i = 0; i < 60; ++i) {
        game.update(1.0f / 60.0f, input);
    }

    CHECK(game.distanceTraveled() > 0.0f);
    CHECK(game.playerPosition().z < 0.0f);
}

TEST_CASE("GlideGame: dirección con input") {
    GlideGame game;
    GlideInput input;
    input.steerX = 1.0f; // Derecha
    input.steerY = 1.0f; // Arriba

    for (int i = 0; i < 20; ++i) {
        game.update(1.0f / 60.0f, input);
    }

    CHECK(game.playerPosition().x > 0.0f);
    CHECK(game.playerPosition().y > 0.0f);
}

TEST_CASE("GlideGame: boost consume energía y se recupera al soltar") {
    GlideGame game;
    GlideInput input;
    input.boost = true;

    // Con boost, la energía debe decaer y la velocidad aumentar
    game.update(0.1f, input);
    CHECK(game.energy() < 100.0f);
    const float boostSpeed = std::abs(game.playerVelocity().z);

    input.boost = false;
    game.update(0.1f, input);
    const float normalSpeed = std::abs(game.playerVelocity().z);
    CHECK(boostSpeed > normalSpeed);

    // Esperar a que recupere energía
    for (int i = 0; i < 150; ++i) {
        game.update(0.05f, input);
    }
    CHECK(game.energy() == doctest::Approx(100.0f));
}

TEST_CASE("GlideGame: recolección de motas de luz y combos") {
    GlideGame game;
    const auto& motes = game.motes();
    REQUIRE_FALSE(motes.empty());

    // Buscamos la posición de la primera mota
    const glm::vec3 targetPos = motes[0].position;

    GlideInput input;
    bool collectedAny = false;
    for (int step = 0; step < 240; ++step) {
        // Guiar hacia la posición de la mota
        if (game.playerPosition().z > targetPos.z) {
            input.steerX = std::clamp((targetPos.x - game.playerPosition().x) * 2.0f, -1.0f, 1.0f);
            input.steerY = std::clamp((targetPos.y - game.playerPosition().y) * 2.0f, -1.0f, 1.0f);
        }
        game.update(1.0f / 60.0f, input);
        for (auto ev : game.audioEvents()) {
            if (ev == AudioEvent::CollectMote) {
                collectedAny = true;
            }
        }
        if (collectedAny) break;
    }

    CHECK(collectedAny);
    CHECK(game.score() > 0);
    CHECK(game.combo() >= 2);
}

TEST_CASE("GlideGame: colisión con cañón, rebote y Game Over tras 3 impactos") {
    GlideGame game;
    GlideInput input;
    input.steerX = 1.0f; // Chocar contra la pared derecha continuamente

    int hits = 0;
    for (int step = 0; step < 600; ++step) {
        game.update(1.0f / 60.0f, input);
        for (auto ev : game.audioEvents()) {
            if (ev == AudioEvent::HitWall) {
                hits++;
            }
        }
        if (game.isGameOver()) {
            break;
        }
    }

    CHECK(hits >= 3);
    CHECK(game.shields() == 0);
    CHECK(game.isGameOver());

    // En GameOver, la posición en Z ya no debe avanzar
    const float finalZ = game.playerPosition().z;
    game.update(0.1f, input);
    CHECK(game.playerPosition().z == doctest::Approx(finalZ));

    // Reinicio con input.restart
    input.restart = true;
    game.update(0.016f, input);
    CHECK_FALSE(game.isGameOver());
    CHECK(game.shields() == 3);
    CHECK(game.score() == 0);
    CHECK(game.distanceTraveled() == doctest::Approx(0.0f));
}

TEST_CASE("Audio procedural de SdfGlide: parámetros SFX válidos y canción de fondo") {
    auto audioParams = GlideAudioParams::create();

    // 1. Recolección de motas con distintos combos
    for (int i = 0; i < 8; ++i) {
        const auto& sfx = audioParams.moteSounds[i];
        mge::Sample sample = mge::renderSfx(sfx, 44100);
        REQUIRE_FALSE(sample.data.empty());

        float sumSq = 0.0f;
        for (float s : sample.data) {
            CHECK_FALSE(std::isnan(s));
            sumSq += s * s;
        }
        CHECK(sumSq > 0.0f);
    }

    // 2. Choque contra pared
    {
        mge::Sample sample = mge::renderSfx(audioParams.hitSound, 44100);
        REQUIRE_FALSE(sample.data.empty());
        for (float s : sample.data) CHECK_FALSE(std::isnan(s));
    }

    // 3. Boost
    {
        mge::Sample sample = mge::renderSfx(audioParams.boostSound, 44100);
        REQUIRE_FALSE(sample.data.empty());
        for (float s : sample.data) CHECK_FALSE(std::isnan(s));
    }

    // 4. Música temática
    const mge::Song& song = audioParams.themeSong;
    CHECK(song.valid());
    CHECK(song.bpm() > 0.0f);
    CHECK(song.lengthSteps() > 0);
}
