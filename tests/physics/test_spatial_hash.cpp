#include <doctest/doctest.h>
#include <mge/core/Random.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/SpatialHash.h>

#include <algorithm>
#include <vector>

using namespace mge;

TEST_CASE(
    "spatialHash: dos cajas solapadas dan un par, cajas tocándose dan un par y cajas lejanas dan cero") {
    SpatialHash hash(2.0f);
    CHECK(hash.cellSize() == 2.0f);

    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;

    // Caso A: Cajas solapadas
    hash.insert(1, Aabb({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}));
    hash.insert(2, Aabb({1.0f, 1.0f, 1.0f}, {3.0f, 3.0f, 3.0f}));
    CHECK(hash.size() == 2);

    hash.computePairs(pairs);
    CHECK(pairs.size() == 1);
    CHECK(pairs[0] == std::make_pair(1u, 2u));

    // Caso B: Cajas tocándose en una cara (comparten x = 2.0)
    hash.clear();
    CHECK(hash.size() == 0);
    hash.insert(10, Aabb({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}));
    hash.insert(20, Aabb({2.0f, 0.0f, 0.0f}, {4.0f, 2.0f, 2.0f}));
    hash.computePairs(pairs);
    CHECK(pairs.size() == 1);
    CHECK(pairs[0] == std::make_pair(10u, 20u));

    // Caso C: Cajas lejanas
    hash.clear();
    hash.insert(100, Aabb({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}));
    hash.insert(200, Aabb({10.0f, 10.0f, 10.0f}, {11.0f, 11.0f, 11.0f}));
    hash.computePairs(pairs);
    CHECK(pairs.empty());
}

TEST_CASE("spatialHash: cajas que abarcan varias celdas no producen pares duplicados") {
    SpatialHash hash(2.0f);

    // Caja grande que abarca múltiples celdas (de -4 a +4 en cada eje = 4x4x4 = 64 celdas)
    hash.insert(1, Aabb({-4.0f, -4.0f, -4.0f}, {4.0f, 4.0f, 4.0f}));
    // Caja pequeña contenida en su interior
    hash.insert(2, Aabb({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}));

    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
    hash.computePairs(pairs);

    // Debe contener el par exactamente una vez
    CHECK(pairs.size() == 1);
    CHECK(pairs[0] == std::make_pair(1u, 2u));
}

TEST_CASE("spatialHash: coordenadas negativas funcionan correctamente en computePairs y query") {
    SpatialHash hash(2.0f);

    hash.insert(5, Aabb({-10.0f, -10.0f, -10.0f}, {-8.0f, -8.0f, -8.0f}));
    hash.insert(6, Aabb({-9.0f, -9.0f, -9.0f}, {-7.0f, -7.0f, -7.0f}));
    hash.insert(7, Aabb({-25.0f, -25.0f, -25.0f}, {-24.0f, -24.0f, -24.0f}));

    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
    hash.computePairs(pairs);
    CHECK(pairs.size() == 1);
    CHECK(pairs[0] == std::make_pair(5u, 6u));

    std::vector<std::uint32_t> ids;
    hash.query(Aabb({-9.5f, -9.5f, -9.5f}, {-8.5f, -8.5f, -8.5f}), ids);
    CHECK(ids.size() == 2);
    CHECK(ids[0] == 5u);
    CHECK(ids[1] == 6u);

    hash.query(Aabb({-24.5f, -24.5f, -24.5f}, {-23.5f, -23.5f, -23.5f}), ids);
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 7u);
}

TEST_CASE("spatialHash: caja que abarca más de 8 celdas por eje funciona correctamente") {
    SpatialHash hash(1.0f);

    // Caja de 12 unidades en X (abarca 12 celdas en X)
    hash.insert(1, Aabb({0.0f, 0.0f, 0.0f}, {12.0f, 1.0f, 1.0f}));
    // Caja tocando el extremo final
    hash.insert(2, Aabb({11.5f, 0.5f, 0.5f}, {13.0f, 1.5f, 1.5f}));

    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
    hash.computePairs(pairs);
    CHECK(pairs.size() == 1);
    CHECK(pairs[0] == std::make_pair(1u, 2u));

    std::vector<std::uint32_t> ids;
    hash.query(Aabb({5.0f, 0.0f, 0.0f}, {6.0f, 1.0f, 1.0f}), ids);
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 1u);
}

TEST_CASE("spatialHash: test aleatorizado de 200 cajas contra fuerza bruta (computePairs, query, clear y "
          "reinsert)") {
    Pcg32 rng(42ull);
    constexpr std::size_t kBoxCount = 200;

    struct TestBox {
        std::uint32_t id = 0;
        Aabb box;
    };
    std::vector<TestBox> testBoxes;
    testBoxes.reserve(kBoxCount);

    SpatialHash hash(2.0f);

    for (std::uint32_t i = 0; i < kBoxCount; ++i) {
        const glm::vec3 pos(-50.0f + rng.nextFloat01() * 100.0f, -50.0f + rng.nextFloat01() * 100.0f,
                            -50.0f + rng.nextFloat01() * 100.0f);
        const glm::vec3 size(0.1f + rng.nextFloat01() * 2.9f, 0.1f + rng.nextFloat01() * 2.9f,
                             0.1f + rng.nextFloat01() * 2.9f);
        const Aabb box(pos, pos + size);
        testBoxes.push_back({i + 1, box});
        hash.insert(i + 1, box);
    }

    CHECK(hash.size() == kBoxCount);

    // 1. Pares por fuerza bruta O(n^2)
    std::vector<std::pair<std::uint32_t, std::uint32_t>> bruteForcePairs;
    for (std::size_t i = 0; i < testBoxes.size(); ++i) {
        for (std::size_t j = i + 1; j < testBoxes.size(); ++j) {
            if (testBoxes[i].box.intersects(testBoxes[j].box)) {
                bruteForcePairs.emplace_back(std::min(testBoxes[i].id, testBoxes[j].id),
                                             std::max(testBoxes[i].id, testBoxes[j].id));
            }
        }
    }
    std::sort(bruteForcePairs.begin(), bruteForcePairs.end());
    bruteForcePairs.erase(std::unique(bruteForcePairs.begin(), bruteForcePairs.end()), bruteForcePairs.end());

    std::vector<std::pair<std::uint32_t, std::uint32_t>> hashPairs;
    hash.computePairs(hashPairs);

    // computePairs coincide exactamente con la lista de fuerza bruta
    CHECK(hashPairs == bruteForcePairs);

    // 2. query() coincide con fuerza bruta para múltiples consultas aleatorias
    for (int q = 0; q < 50; ++q) {
        const glm::vec3 qPos(-50.0f + rng.nextFloat01() * 100.0f, -50.0f + rng.nextFloat01() * 100.0f,
                             -50.0f + rng.nextFloat01() * 100.0f);
        const glm::vec3 qSize(0.1f + rng.nextFloat01() * 4.0f, 0.1f + rng.nextFloat01() * 4.0f,
                              0.1f + rng.nextFloat01() * 4.0f);
        const Aabb qBox(qPos, qPos + qSize);

        std::vector<std::uint32_t> bruteForceIds;
        for (const auto& item : testBoxes) {
            if (item.box.intersects(qBox)) {
                bruteForceIds.push_back(item.id);
            }
        }
        std::sort(bruteForceIds.begin(), bruteForceIds.end());
        bruteForceIds.erase(std::unique(bruteForceIds.begin(), bruteForceIds.end()), bruteForceIds.end());

        std::vector<std::uint32_t> hashIds;
        hash.query(qBox, hashIds);

        CHECK(hashIds == bruteForceIds);
    }

    // 3. clear() y reinsert da el mismo resultado idéntico
    hash.clear();
    CHECK(hash.size() == 0);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> clearedPairs;
    hash.computePairs(clearedPairs);
    CHECK(clearedPairs.empty());

    for (const auto& item : testBoxes) {
        hash.insert(item.id, item.box);
    }
    CHECK(hash.size() == kBoxCount);

    std::vector<std::pair<std::uint32_t, std::uint32_t>> reinsertedPairs;
    hash.computePairs(reinsertedPairs);
    CHECK(reinsertedPairs == bruteForcePairs);
}
