#include <doctest/doctest.h>
#include <examples/platformer2d/Platformer.h>
#include <mge/core/Assets.h>

#include <string>

using namespace platformer;

namespace {
    // Un piso de 12 casillas con un pozo, una moneda arriba y un enemigo
    const char* kSmallLevel = "#BIOME GRASS\n"
                              "............\n"
                              "............\n"
                              ".@..?....E..\n"
                              "............\n"
                              "GGGG[-]GG.GG\n"
                              "DDDD...DD.DD\n";

    Platformer smallGame() {
        std::string error;
        auto level = parseLevel(kSmallLevel, &error);
        REQUIRE_MESSAGE(level.has_value(), error);
        return Platformer(std::move(*level));
    }

    // Corre n pasos de 1/60 s con la misma entrada; devuelve los eventos sumados
    StepEvents run(Platformer& game, const PlayerInput& input, int steps) {
        StepEvents total;
        for (int i = 0; i < steps; i++) {
            const StepEvents e = game.step(input, 1.0f / 60.0f);
            total.coins += e.coins;
            total.jumped = total.jumped || e.jumped;
            total.died = total.died || e.died;
            total.finished = total.finished || e.finished;
        }
        return total;
    }
}

TEST_CASE("parseLevel lee bioma, casillas, inicio, objetos y enemigos") {
    std::string error;
    const auto level = parseLevel(kSmallLevel, &error);
    REQUIRE_MESSAGE(level.has_value(), error);
    CHECK(level->biome == "GRASS");
    CHECK(level->width == 12);
    CHECK(level->height == 6);
    CHECK(level->spawn == glm::ivec2(1, 2));
    REQUIRE(level->items.size() == 1);
    CHECK(level->items[0].cell == glm::ivec2(4, 2));
    CHECK(level->items[0].kind == ItemKind::Coin);
    REQUIRE(level->enemies.size() == 1);
    CHECK(level->at(0, 4) == Tile::Ground);
    CHECK(level->at(4, 4) == Tile::BridgeLeft);
    CHECK(level->at(5, 4) == Tile::Bridge);
    CHECK(level->at(0, 5) == Tile::Dirt);
    CHECK(level->at(9, 4) == Tile::Empty); // El pozo
    CHECK(level->at(1, 2) == Tile::Empty); // '@' no es sólido
    CHECK_FALSE(level->solid(-1, 4));      // Fuera: vacío
    CHECK_FALSE(level->solid(0, 100));
}

TEST_CASE("parseLevel rechaza niveles sin filas, sin inicio o con caracteres desconocidos") {
    std::string error;
    CHECK_FALSE(parseLevel("", &error));
    CHECK_FALSE(parseLevel("#BIOME ICE\n", &error));
    CHECK_FALSE(parseLevel("....\nGGGG\n", &error));
    CHECK(error.find('@') != std::string::npos);
    CHECK_FALSE(parseLevel(".@.X\nGGGG\n", &error));
    CHECK(error.find("'X'") != std::string::npos);
    CHECK(parseLevel(".@\r\nGG\r\n")); // Finales de línea de Windows
}

TEST_CASE("Los cuatro niveles de assets/levels se leen") {
    for (int i = 1; i <= 4; i++) {
        CAPTURE(i);
        std::string error;
        const auto level = loadLevel("levels/level" + std::to_string(i) + ".txt", &error);
        REQUIRE_MESSAGE(level.has_value(), error);
        CHECK(level->height == 10);
        CHECK(level->width >= 68);
        CHECK_FALSE(level->biome.empty());
        CHECK_FALSE(level->items.empty());
    }
}

TEST_CASE("El jugador cae al piso, camina, salta y no atraviesa paredes") {
    Platformer game = smallGame();
    run(game, {}, 60);
    CHECK(game.grounded());
    // Parado sobre la fila 4: los pies en y = 4
    CHECK(game.playerPosition().y + Platformer::kPlayerSize.y == doctest::Approx(4.0f).epsilon(1e-3));

    const float startX = game.playerPosition().x;
    run(game, {1.0f, false}, 10);
    CHECK(game.playerPosition().x > startX + 0.5f);

    const StepEvents jump = run(game, {0.0f, true}, 1);
    CHECK(jump.jumped);
    CHECK_FALSE(game.grounded());
    float top = game.playerPosition().y;
    for (int i = 0; i < 40; i++) {
        game.step({}, 1.0f / 60.0f);
        top = std::min(top, game.playerPosition().y);
    }
    CHECK(top < 4.0f - Platformer::kPlayerSize.y - 2.0f); // Más de dos casillas de salto
    run(game, {}, 60);
    CHECK(game.grounded());

    // Hacia la izquierda, la pared invisible del borde
    run(game, {-1.0f, false}, 120);
    CHECK(game.playerPosition().x == doctest::Approx(0.0f));
}

TEST_CASE("Monedas al tocarlas, caídas al vacío y llegada al final") {
    Platformer game = smallGame();
    run(game, {}, 30);
    // Saltar a la moneda de la casilla (4, 2)
    StepEvents events = run(game, {1.0f, false}, 20);
    events = run(game, {0.4f, true}, 30);
    CHECK(game.coins() == 1);
    CHECK(game.collected(0));
    CHECK(events.coins == 1);
    CHECK(run(game, {}, 60).coins == 0); // No se cuenta dos veces

    // Caminando a la derecha: el enemigo (cayó al piso en la columna 9, que es el pozo → fondo del nivel)
    // o el pozo te devuelven al inicio
    events = run(game, {1.0f, false}, 240);
    CHECK(events.died);
    CHECK(game.deaths() >= 1);
}

TEST_CASE("Al llegar al borde derecho el nivel termina") {
    auto level = parseLevel(".@......\nGGGGGGGG\n");
    REQUIRE(level);
    Platformer game(std::move(*level));
    CHECK(run(game, {1.0f, false}, 120).finished);
}
