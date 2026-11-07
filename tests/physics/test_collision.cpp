#include <doctest/doctest.h>
#include <mge/physics/GridQuery.h>
#include <mge/physics/VoxelCollision.h>
#include <mge/voxel/VoxelWorld.h>

#include <thread>
#include <unordered_set>
#include <vector>

using namespace mge;

namespace {

    // Bloques de prueba mínimos para VoxelWorld
    struct TestBlockRegistry {
        BlockRegistry registry;
        BlockId stone = 0;

        TestBlockRegistry() {
            BlockDesc desc;
            desc.name = "stone";
            desc.solid = true;
            stone = registry.add(desc);
        }
    };

    // Hash para consultar un conjunto local de bloques sólidos en tests sin instanciar VoxelWorld
    struct Ivec3Hash {
        std::size_t operator()(const glm::ivec3& p) const {
            std::size_t h = std::hash<int>()(p.x);
            h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(p.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

} // namespace

TEST_CASE("moveAabb: caja cayendo hacia el suelo frena a 1.0 + skin y queda grounded") {
    // Suelo en y = 0 (el bloque ocupa y in [0, 1), por lo que la cara superior está en y = 1.0)
    auto isSolid = [](const glm::ivec3& b) { return b.y == 0; };

    // Caja de 0.6 x 1.8 x 0.6 cayendo desde y = 5.0
    Aabb box({-0.3f, 5.0f, -0.3f}, {0.3f, 6.8f, 0.3f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {0.0f, -10.0f, 0.0f}, isSolid, opts);

    CHECK(res.grounded);
    CHECK(res.blocked.y);
    CHECK_FALSE(res.blocked.x);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.min.y == doctest::Approx(1.0f + opts.skin));
    CHECK(res.box.max.y == doctest::Approx(2.8f + opts.skin));
    CHECK(res.moved.y == doctest::Approx(1.0f + opts.skin - 5.0f));
}

TEST_CASE("moveAabb: sin tunneling con delta enorme (+50) contra pared de 1 bloque") {
    // Pared de 1 bloque de espesor en x = 10 (ocupa [10, 11))
    auto isSolid = [](const glm::ivec3& b) { return b.x == 10; };

    Aabb box({0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {50.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK_FALSE(res.blocked.y);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.max.x == doctest::Approx(10.0f - opts.skin));
    CHECK(res.box.min.x == doctest::Approx(10.0f - opts.skin - 0.6f));
    CHECK(res.moved.x == doctest::Approx(10.0f - opts.skin - 0.6f));
}

TEST_CASE("moveAabb: deslizamiento en pared (movimiento diagonal conserva eje libre)") {
    // Pared a lo largo de x = 5 (bloques x >= 5 son sólidos)
    auto isSolid = [](const glm::ivec3& b) { return b.x >= 5; };

    Aabb box({0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    // Desplazamiento diagonal hacia +X y +Z
    const MoveResult res = moveAabb(box, {10.0f, 0.0f, 3.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.max.x == doctest::Approx(5.0f - opts.skin));
    CHECK(res.moved.z == doctest::Approx(3.0f));
    CHECK(res.box.min.z == doctest::Approx(3.0f));
}

TEST_CASE("moveAabb: caja en contacto exacto con la pared no retrocede y puede alejarse") {
    auto isSolid = [](const glm::ivec3& b) { return b.x >= 10; };

    // Caja tocando exactamente la pared en max.x = 10.0
    Aabb box({9.4f, 0.0f, 0.0f}, {10.0f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    SUBCASE("Mover hacia la pared no avanza ni retrocede") {
        const MoveResult res = moveAabb(box, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK(res.blocked.x);
        CHECK(res.moved.x == 0.0f);
        CHECK(res.box.max.x == doctest::Approx(10.0f));
    }

    SUBCASE("Mover alejándose de la pared es completamente libre") {
        const MoveResult res = moveAabb(box, {-1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.blocked.x);
        CHECK(res.moved.x == doctest::Approx(-1.0f));
        CHECK(res.box.max.x == doctest::Approx(9.0f));
    }
}

TEST_CASE("moveAabb: caja que arranca dentro de un bloque sólido puede salir") {
    // Solo el bloque (0, 0, 0) es sólido
    auto isSolid = [](const glm::ivec3& b) { return b == glm::ivec3(0, 0, 0); };

    // Caja situada dentro de [0, 1)³
    Aabb box({0.2f, 0.2f, 0.2f}, {0.8f, 0.8f, 0.8f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {2.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK_FALSE(res.blocked.x);
    CHECK(res.moved.x == doctest::Approx(2.0f));
    CHECK(res.box.min.x == doctest::Approx(2.2f));
}

TEST_CASE("moveAabb: colisión en coordenadas negativas") {
    // Pared en x = -20 (bloque ocupa [-20, -19), cara este en -19.0)
    auto isSolid = [](const glm::ivec3& b) { return b.x == -20; };

    Aabb box({-15.0f, 0.0f, 0.0f}, {-14.4f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {-10.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK(res.box.min.x == doctest::Approx(-19.0f + opts.skin));
    CHECK(res.moved.x == doctest::Approx(-19.0f + opts.skin - (-15.0f)));
}

TEST_CASE("moveAabb: subida de escalón (step-up)") {
    // Suelo en y = 0 (top en 1.0) para x en [0..4]
    // Escalón en x = 2: bloque (2, 1, 0) es sólido (top en 2.0)
    std::unordered_set<glm::ivec3, Ivec3Hash> solids;
    for (int x = 0; x <= 5; ++x) {
        solids.insert({x, 0, 0});
    }
    solids.insert({2, 1, 0}); // 1 bloque de altura

    auto isSolid = [&solids](const glm::ivec3& b) { return solids.count(b) != 0; };

    MoveOptions opts;
    opts.skin = 1e-4f;

    // Caja apoyada en el suelo en x = 1 (max.x = 1.6, min.y = 1.0 + skin)
    const Aabb groundedBox({1.0f, 1.0f + opts.skin, 0.0f}, {1.6f, 2.8f + opts.skin, 0.6f});

    SUBCASE("stepHeight 1.0 sube el escalón de 1 bloque") {
        opts.stepHeight = 1.0f;
        const MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);

        CHECK(res.stepped);
        CHECK(res.grounded);
        // Debe terminar sobre el escalón: min.y ≈ 2.0 + skin
        CHECK(res.box.min.y == doctest::Approx(2.0f + opts.skin));
        CHECK(res.box.min.x > 1.0f);
    }

    SUBCASE("Pared de 2 bloques no se sube con stepHeight 1.0") {
        solids.insert({2, 2, 0}); // Pared ahora de 2 bloques
        opts.stepHeight = 1.0f;
        const MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);

        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);
        CHECK(res.box.max.x == doctest::Approx(2.0f - opts.skin));
    }

    SUBCASE("stepHeight 0.6 o 0 no sube un bloque entero de 1.0") {
        opts.stepHeight = 0.6f;
        MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);

        opts.stepHeight = 0.0f;
        res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);
    }
}

TEST_CASE("moveAabb: sobrecarga con VoxelWorld (cruza borde de chunk y chunks sin cargar son aire)") {
    TestBlockRegistry reg;
    VoxelWorld world(reg.registry);

    // Cargar chunk {0, 0, 0} y chunk {1, 0, 0}
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0});

    // Pared exactamente en x = 16 (primer bloque de chunk {1, 0, 0})
    world.setBlock({16, 2, 0}, reg.stone);

    Aabb box({14.0f, 2.0f, 0.0f}, {14.6f, 3.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    SUBCASE("Frena contra la pared en x = 16 al cruzar desde chunk 0") {
        const MoveResult res = moveAabb(world, box, {5.0f, 0.0f, 0.0f}, opts);
        CHECK(res.blocked.x);
        CHECK(res.box.max.x == doctest::Approx(16.0f - opts.skin));
    }

    SUBCASE("Chunks sin cargar se consideran aire y se atraviesan sin fallos") {
        // Mover hacia +Z (chunk {0, 0, 1} no está cargado)
        const MoveResult res = moveAabb(world, box, {0.0f, 0.0f, 30.0f}, opts);
        CHECK_FALSE(res.blocked.z);
        CHECK(res.moved.z == doctest::Approx(30.0f));
    }
}

TEST_CASE("moveAabb: clamping de delta concurrente es seguro en hilos (atomic warnedClamp)") {
    auto isSolid = [](const glm::ivec3& b) { return b.y <= 0; };
    Aabb box({0.0f, 10.0f, 0.0f}, {1.0f, 11.0f, 1.0f});

    constexpr int kNumThreads = 8;
    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 50; ++i) {
                const MoveResult res = moveAabb(box, {500.0f, 0.0f, 0.0f}, isSolid);
                CHECK(res.moved.x <= 256.0f);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}

TEST_CASE("VoxelCollision: deteccion de solapamiento y retroceso de spawn contra pared") {
    TestBlockRegistry reg;
    VoxelWorld world(reg.registry);
    world.loadChunk({0, 0, 0});

    // Pared de piedra en x = 2 (bloque [2, 3])
    world.setBlock({2, 1, 0}, reg.stone);

    auto overlapsSolid = [&](const Aabb& aabb) {
        constexpr float kTiny = 1e-4f;
        const int minX = static_cast<int>(std::floor(aabb.min.x + kTiny));
        const int maxX = static_cast<int>(std::floor(aabb.max.x - kTiny));
        const int minY = static_cast<int>(std::floor(aabb.min.y + kTiny));
        const int maxY = static_cast<int>(std::floor(aabb.max.y - kTiny));
        const int minZ = static_cast<int>(std::floor(aabb.min.z + kTiny));
        const int maxZ = static_cast<int>(std::floor(aabb.max.z - kTiny));
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                for (int x = minX; x <= maxX; ++x) {
                    if (world.isSolid(glm::ivec3(x, y, z))) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const glm::vec3 halfExtents(0.25f);
    const glm::vec3 fwd(1.0f, 0.0f, 0.0f); // Mirando hacia la pared en x = 2

    // Jugador parado muy cerca de la pared; spawn en (1.9, 1.5, 0.5)
    // Box abarcaría x in [1.65, 2.15], que penetra el bloque en x = 2
    const glm::vec3 eyePos(1.9f, 1.5f, 0.5f);
    const Aabb initialBounds(eyePos - halfExtents, eyePos + halfExtents);
    CHECK(overlapsSolid(initialBounds));

    // Retroceso a lo largo de -forward() hasta 0.5 bloques
    glm::vec3 spawnPos = eyePos;
    constexpr float kMaxBacktrack = 0.5f;
    constexpr int kSteps = 10;
    bool foundFree = false;
    for (int i = 1; i <= kSteps; ++i) {
        const float backDist = (kMaxBacktrack / static_cast<float>(kSteps)) * static_cast<float>(i);
        const glm::vec3 candidatePos = eyePos - fwd * backDist;
        const Aabb candidateBounds(candidatePos - halfExtents, candidatePos + halfExtents);
        if (!overlapsSolid(candidateBounds)) {
            spawnPos = candidatePos;
            foundFree = true;
            break;
        }
    }

    CHECK(foundFree);
    CHECK(spawnPos.x <= 1.75f);
    CHECK_FALSE(overlapsSolid(Aabb(spawnPos - halfExtents, spawnPos + halfExtents)));
}
