#include <doctest/doctest.h>
#include <examples/platformer2d/Platformer.h>
#include <mge/core/Assets.h>

#include <string>

using namespace platformer;

namespace {
    // A 12-tile floor with a pit, a coin above, and an enemy
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

    // Runs n steps of 1/60 s with the same input; returns accumulated events
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

TEST_CASE("parseLevel reads biome, tiles, spawn, items, and enemies") {
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
    CHECK(level->at(9, 4) == Tile::Empty); // The pit
    CHECK(level->at(1, 2) == Tile::Empty); // '@' is not solid
    CHECK_FALSE(level->solid(-1, 4));      // Out of bounds: empty
    CHECK_FALSE(level->solid(0, 100));
}

TEST_CASE("parseLevel rejects levels without rows, without spawn, or with unknown characters") {
    std::string error;
    CHECK_FALSE(parseLevel("", &error));
    CHECK_FALSE(parseLevel("#BIOME ICE\n", &error));
    CHECK_FALSE(parseLevel("....\nGGGG\n", &error));
    CHECK(error.find('@') != std::string::npos);
    CHECK_FALSE(parseLevel(".@.X\nGGGG\n", &error));
    CHECK(error.find("'X'") != std::string::npos);
    CHECK(parseLevel(".@\r\nGG\r\n")); // Windows line endings
}

TEST_CASE("All four levels in assets/levels can be loaded") {
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

TEST_CASE("loadLevel with nonexistent file returns nullopt and reports error") {
    std::string error;
    CHECK_FALSE(loadLevel("levels/no_existe.txt", &error).has_value());
    CHECK(error.find("no se pudo abrir") != std::string::npos);
}

TEST_CASE("Player falls to the floor, walks, jumps, and does not pass through walls") {
    Platformer game = smallGame();
    run(game, {}, 60);
    CHECK(game.grounded());
    // Standing on row 4: feet at y = 4
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
    CHECK(top < 4.0f - Platformer::kPlayerSize.y - 2.0f); // Jump higher than two tiles
    run(game, {}, 60);
    CHECK(game.grounded());

    // To the left, the invisible boundary wall
    run(game, {-1.0f, false}, 120);
    CHECK(game.playerPosition().x == doctest::Approx(0.0f));
}

TEST_CASE("Coins on touch, falling into pits, and reaching the goal") {
    Platformer game = smallGame();
    run(game, {}, 30);
    // Jump to the coin at tile (4, 2)
    StepEvents events = run(game, {1.0f, false}, 20);
    events = run(game, {0.4f, true}, 30);
    CHECK(game.coins() == 1);
    CHECK(game.collected(0));
    CHECK(events.coins == 1);
    CHECK(run(game, {}, 60).coins == 0); // Not counted twice

    // Walking right: the enemy (fell to the floor in column 9, which is the pit -> bottom of the level)
    // or the pit sends you back to spawn
    events = run(game, {1.0f, false}, 240);
    CHECK(events.died);
    CHECK(game.deaths() >= 1);
}

TEST_CASE("Reaching the right edge finishes the level") {
    auto level = parseLevel(".@......\nGGGGGGGG\n");
    REQUIRE(level);
    Platformer game(std::move(*level));
    CHECK(run(game, {1.0f, false}, 120).finished);
}
