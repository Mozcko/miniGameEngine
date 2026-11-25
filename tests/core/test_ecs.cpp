#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;

namespace {
    struct Position {
        float x = 0.0f;
        float y = 0.0f;
    };
    struct Velocity {
        float dx = 0.0f;
        float dy = 0.0f;
    };
    struct Tag {};
    struct Named {
        std::string name; // Non-trivial component (moved during swap-and-pop)
    };
}

TEST_CASE("Entities: create, destroy, recycle indices, and invalidate old handles") {
    Registry registry;
    Entity a = registry.create();
    Entity b = registry.create();
    CHECK(registry.valid(a));
    CHECK(registry.valid(b));
    CHECK(a != b);
    CHECK(registry.aliveCount() == 2);

    registry.destroy(a);
    CHECK_FALSE(registry.valid(a));
    CHECK(registry.aliveCount() == 1);

    Entity c = registry.create(); // Reuses a's index with a new generation
    CHECK(c.index == a.index);
    CHECK(c.generation != a.generation);
    CHECK(registry.valid(c));
    CHECK_FALSE(registry.valid(a)); // Old handle remains invalid

    registry.destroy(a); // Destroying an invalid handle is a no-op
    CHECK(registry.valid(c));
    CHECK_FALSE(registry.valid(kNullEntity));
}

TEST_CASE("Components: add, get, has, tryGet, remove, replace") {
    Registry registry;
    Entity e = registry.create();
    CHECK_FALSE(registry.has<Position>(e));
    CHECK(registry.tryGet<Position>(e) == nullptr);

    registry.add<Position>(e, Position{1.0f, 2.0f});
    CHECK(registry.has<Position>(e));
    CHECK(registry.get<Position>(e).y == doctest::Approx(2.0f));
    registry.get<Position>(e).x = 5.0f;
    CHECK(registry.tryGet<Position>(e)->x == doctest::Approx(5.0f));

    registry.add<Position>(e, Position{9.0f, 9.0f}); // Replaces
    CHECK(registry.count<Position>() == 1);
    CHECK(registry.get<Position>(e).x == doctest::Approx(9.0f));

    registry.remove<Position>(e);
    CHECK_FALSE(registry.has<Position>(e));
    CHECK(registry.count<Position>() == 0);
    registry.remove<Position>(e); // Removing a missing component is a no-op
}

TEST_CASE("Destroying an entity removes all its components without breaking others (swap-and-pop)") {
    Registry registry;
    std::vector<Entity> entities;
    for (int i = 0; i < 5; i++) {
        Entity e = registry.create();
        registry.add<Named>(e, Named{"e" + std::to_string(i)});
        registry.add<Position>(e, Position{static_cast<float>(i), 0.0f});
        entities.push_back(e);
    }
    registry.destroy(entities[1]); // From the middle: the last element takes its slot
    CHECK(registry.count<Named>() == 4);
    CHECK(registry.get<Named>(entities[4]).name == "e4");
    CHECK(registry.get<Position>(entities[4]).x == doctest::Approx(4.0f));
    CHECK(registry.get<Named>(entities[0]).name == "e0");

    // A new entity at the recycled index does not inherit components
    Entity recycled = registry.create();
    CHECK(recycled.index == entities[1].index);
    CHECK_FALSE(registry.has<Named>(recycled));
}

TEST_CASE("view<A, B> iterates only over entities with both components") {
    Registry registry;
    std::set<std::uint32_t> withBoth;
    for (int i = 0; i < 10; i++) {
        Entity e = registry.create();
        registry.add<Position>(e);
        if (i % 3 == 0) {
            registry.add<Velocity>(e, Velocity{1.0f, 2.0f});
            withBoth.insert(e.index);
        }
        if (i == 9) registry.add<Tag>(e);
    }

    std::set<std::uint32_t> visited;
    registry.each<Position, Velocity>([&](Entity e, Position& p, Velocity& v) {
        visited.insert(e.index);
        p.x += v.dx; // Can mutate inside each
    });
    CHECK(visited == withBoth);
    CHECK(registry.view<Position, Velocity>().count() == 4);
    CHECK(registry.view<Position, Tag>().count() == 1);
    CHECK(registry.view<Position, Named>().count() == 0); // Type without a pool yet
}

TEST_CASE("TransformSystem: world matrices with hierarchy (parents before children)") {
    Registry registry;
    Entity root = registry.create();
    Entity child = registry.create();
    Entity grandchild = registry.create();

    registry.add<Transform>(root, Transform{{10.0f, 0.0f, 0.0f}});
    Transform childTransform;
    childTransform.position = {0.0f, 5.0f, 0.0f};
    childTransform.scale = glm::vec3(2.0f);
    registry.add<Transform>(child, childTransform);
    registry.add<Parent>(child, Parent{root});
    registry.add<Transform>(grandchild, Transform{{1.0f, 0.0f, 0.0f}});
    registry.add<Parent>(grandchild, Parent{child});

    TransformSystem::update(registry);

    auto origin = [&](Entity e) {
        return glm::vec3(registry.get<WorldTransform>(e).matrix * glm::vec4(0, 0, 0, 1));
    };
    CHECK(origin(root).x == doctest::Approx(10.0f));
    CHECK(origin(child).x == doctest::Approx(10.0f));
    CHECK(origin(child).y == doctest::Approx(5.0f));
    // Grandchild inherits child's scale of 2: its local (1,0,0) ends up 2 units away
    CHECK(origin(grandchild).x == doctest::Approx(12.0f));
    CHECK(origin(grandchild).y == doctest::Approx(5.0f));
}

TEST_CASE("TransformSystem: rotation, destroyed parent, and cycles") {
    Registry registry;
    Entity pivot = registry.create();
    Transform rotated;
    rotated.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    registry.add<Transform>(pivot, rotated);
    Entity arm = registry.create();
    registry.add<Transform>(arm, Transform{{1.0f, 0.0f, 0.0f}});
    registry.add<Parent>(arm, Parent{pivot});

    // Cycle a <-> b: must not hang
    Entity a = registry.create();
    Entity b = registry.create();
    registry.add<Transform>(a);
    registry.add<Transform>(b);
    registry.add<Parent>(a, Parent{b});
    registry.add<Parent>(b, Parent{a});

    Log::setSink([](const LogRecord&) {}); // Silence cycle warning
    TransformSystem::update(registry);
    Log::setSink(nullptr);

    glm::vec3 armWorld = glm::vec3(registry.get<WorldTransform>(arm).matrix * glm::vec4(0, 0, 0, 1));
    CHECK(armWorld.x == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(armWorld.z == doctest::Approx(-1.0f)); // +X rotated 90° around Y -> -Z
    CHECK(registry.has<WorldTransform>(a));

    // Destroyed parent: child becomes a root
    registry.destroy(pivot);
    TransformSystem::update(registry);
    armWorld = glm::vec3(registry.get<WorldTransform>(arm).matrix * glm::vec4(0, 0, 0, 1));
    CHECK(armWorld.x == doctest::Approx(1.0f));
}

TEST_CASE("World: systems run by phase and in registration order") {
    World world;
    std::vector<std::string> calls;
    world.addSystem(SystemPhase::Update, "a", [&](World&, float) { calls.push_back("update.a"); });
    world.addSystem(SystemPhase::Fixed, "physics", [&](World&, float) { calls.push_back("fixed"); });
    world.addSystem(SystemPhase::Update, "b", [&](World&, float) { calls.push_back("update.b"); });
    world.run(SystemPhase::Update, 0.016f);
    world.run(SystemPhase::Render, 0.016f); // No systems
    std::vector<std::string> expected = {"update.a", "update.b"};
    CHECK(calls == expected);
    CHECK(world.systemCount(SystemPhase::Fixed) == 1);
}

TEST_CASE("A headless app runs World systems in their phases for N frames") {
    class EcsApp : public Application {
    public:
        EcsApp() : Application(makeConfig()) { setClock(std::make_unique<ManualClock>(1.0 / 60.0)); }
        int updateSystems = 0;
        int renderSystems = 0;

    protected:
        bool onInit() override {
            Registry& registry = world().registry();
            for (int i = 0; i < 3; i++) {
                Entity e = registry.create();
                registry.add<Position>(e);
                registry.add<Velocity>(e, Velocity{1.0f, 0.0f});
            }
            // Movement in fixed step: deterministic
            world().addSystem(SystemPhase::Fixed, "movement", [](World& w, float dt) {
                w.registry().each<Position, Velocity>(
                    [dt](Entity, Position& p, Velocity& v) { p.x += v.dx * dt; });
            });
            world().addSystem(SystemPhase::Update, "count", [this](World&, float) { updateSystems++; });
            world().addSystem(SystemPhase::Render, "count", [this](World&, float) { renderSystems++; });
            return true;
        }

    private:
        static ApplicationConfig makeConfig() {
            ApplicationConfig config;
            config.headless = true;
            config.maxFrames = 60;
            return config;
        }
    };

    EcsApp app;
    CHECK(app.run() == 0);
    CHECK(app.updateSystems == 60);
    CHECK(app.renderSystems == 60);
    // 60 fixed steps of 1/60 s at 1 u/s = 1 unit
    app.world().registry().each<Position>([](Entity, Position& p) { CHECK(p.x == doctest::Approx(1.0f)); });
}

TEST_CASE("ECS runtime: strict 16-byte and 32-byte alignment") {
    Registry registry;
    const ComponentTypeId align16 = registry.registerRuntimeComponent("Align16", 24, 16);
    const ComponentTypeId align32 = registry.registerRuntimeComponent("Align32", 48, 32);

    for (int i = 0; i < 20; ++i) {
        Entity e = registry.create();
        void* p16 = registry.addRaw(e, align16);
        void* p32 = registry.addRaw(e, align32);

        CHECK(reinterpret_cast<std::uintptr_t>(p16) % 16 == 0);
        CHECK(reinterpret_cast<std::uintptr_t>(p32) % 32 == 0);
    }
}

TEST_CASE("ECS runtime: addRaw, getRaw, hasRaw, removeRaw operations") {
    Registry registry;
    struct CustomData {
        float x, y, z;
        int id;
    };
    const ComponentTypeId customType =
        registry.registerRuntimeComponent("Custom", sizeof(CustomData), alignof(CustomData));

    Entity e1 = registry.create();
    Entity e2 = registry.create();

    CHECK_FALSE(registry.hasRaw(e1, customType));
    CHECK_FALSE(registry.hasRaw(e2, customType));

    // addRaw zero-initializes memory
    auto* d1 = static_cast<CustomData*>(registry.addRaw(e1, customType));
    REQUIRE(d1 != nullptr);
    CHECK(d1->x == 0.0f);
    CHECK(d1->y == 0.0f);
    CHECK(d1->z == 0.0f);
    CHECK(d1->id == 0);

    d1->x = 10.0f;
    d1->id = 42;

    CHECK(registry.hasRaw(e1, customType));
    auto* fetched = static_cast<CustomData*>(registry.getRaw(e1, customType));
    CHECK(fetched->x == 10.0f);
    CHECK(fetched->id == 42);

    // removeRaw
    CHECK(registry.removeRaw(e1, customType));
    CHECK_FALSE(registry.hasRaw(e1, customType));
    CHECK_FALSE(registry.removeRaw(e1, customType));
}

TEST_CASE("ECS runtime: eachRaw with multiple components") {
    Registry registry;
    const ComponentTypeId tA = registry.registerRuntimeComponent("TypeA", sizeof(int), alignof(int));
    const ComponentTypeId tB = registry.registerRuntimeComponent("TypeB", sizeof(float), alignof(float));

    Entity eBoth1 = registry.create();
    Entity eAOnly = registry.create();
    Entity eBoth2 = registry.create();
    Entity eBOnly = registry.create();

    *static_cast<int*>(registry.addRaw(eBoth1, tA)) = 100;
    *static_cast<float*>(registry.addRaw(eBoth1, tB)) = 1.5f;

    *static_cast<int*>(registry.addRaw(eAOnly, tA)) = 200;

    *static_cast<int*>(registry.addRaw(eBoth2, tA)) = 300;
    *static_cast<float*>(registry.addRaw(eBoth2, tB)) = 3.5f;

    *static_cast<float*>(registry.addRaw(eBOnly, tB)) = 4.5f;

    std::vector<ComponentTypeId> types = {tA, tB};
    int count = 0;
    int sumInt = 0;
    float sumFloat = 0.0f;

    registry.eachRaw(types, [&](Entity e, void** ptrs) {
        count++;
        auto* a = static_cast<int*>(ptrs[0]);
        auto* b = static_cast<float*>(ptrs[1]);
        sumInt += *a;
        sumFloat += *b;
        CHECK((e == eBoth1 || e == eBoth2));
    });

    CHECK(count == 2);
    CHECK(sumInt == 400);
    CHECK(sumFloat == doctest::Approx(5.0f));
}

TEST_CASE("ECS runtime: destroying an entity cleans up raw components") {
    Registry registry;
    const ComponentTypeId typeId = registry.registerRuntimeComponent("Test", sizeof(double), alignof(double));

    Entity e = registry.create();
    registry.addRaw(e, typeId);
    CHECK(registry.hasRaw(e, typeId));

    registry.destroy(e);
    CHECK_FALSE(registry.hasRaw(e, typeId));
}

TEST_CASE("ECS runtime: registering many component types") {
    Registry registry;
    std::vector<ComponentTypeId> ids;
    ids.reserve(50);
    for (int i = 0; i < 50; ++i) {
        ids.push_back(
            registry.registerRuntimeComponent("Type_" + std::to_string(i), sizeof(int), alignof(int)));
    }

    Entity e = registry.create();
    for (auto id : ids) {
        *static_cast<int*>(registry.addRaw(e, id)) = static_cast<int>(id);
    }

    for (auto id : ids) {
        CHECK(registry.hasRaw(e, id));
        CHECK(*static_cast<int*>(registry.getRaw(e, id)) == static_cast<int>(id));
    }
}

TEST_CASE("ECS runtime: size, alignment, and conflict validation in registerRuntimeComponent") {
    Registry registry;

    // align == 0 uses alignof(std::max_align_t)
    const ComponentTypeId defaultAlignId = registry.registerRuntimeComponent("DefaultAlign", 16, 0);
    CHECK(defaultAlignId != kInvalidComponentType);
    Entity e = registry.create();
    void* p = registry.addRaw(e, defaultAlignId);
    REQUIRE(p != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t) == 0);

    // Same name and same layout → idempotent
    const ComponentTypeId velId = registry.registerRuntimeComponent("Velocity", 12, 4);
    CHECK(velId != kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("Velocity", 12, 4) == velId);

    // Same name with different size or alignment → error (does not collide or overflow)
    CHECK(registry.registerRuntimeComponent("Velocity", 64, 4) == kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("Velocity", 12, 16) == kInvalidComponentType);

    // Empty name or out-of-range parameters
    CHECK(registry.registerRuntimeComponent("", 16, 4) == kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("BadAlign3", 16, 3) == kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("BadAlign6", 16, 6) == kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("BadAlignHuge", 16, 8192) == kInvalidComponentType);
    CHECK(registry.registerRuntimeComponent("TooLarge", 1024u * 1024u, 16) == kInvalidComponentType);
}

TEST_CASE("ECS runtime: invalid IDs and invalid entities return null or false in Release") {
    Registry registry;
    Entity e = registry.create();
    constexpr ComponentTypeId kUnknownId = 999;

    CHECK_FALSE(registry.isValidRuntimeComponent(kUnknownId));
    CHECK_FALSE(registry.isValidRuntimeComponent(kInvalidComponentType));
    CHECK(registry.addRaw(e, kUnknownId) == nullptr);
    CHECK(registry.addRaw(e, kInvalidComponentType) == nullptr);
    CHECK(registry.getRaw(e, kUnknownId) == nullptr);
    CHECK_FALSE(registry.hasRaw(e, kUnknownId));
    CHECK_FALSE(registry.removeRaw(e, kUnknownId));

    const ComponentTypeId validId = registry.registerRuntimeComponent("Flag", 4, 4);
    Entity dead = registry.create();
    registry.destroy(dead);
    CHECK(registry.addRaw(dead, validId) == nullptr);
    CHECK(registry.getRaw(dead, validId) == nullptr);
    CHECK_FALSE(registry.hasRaw(dead, validId));
}

TEST_CASE("ECS runtime and templated: safe add-during-each and destroy-during-each") {
    Registry registry;
    const ComponentTypeId valId = registry.registerRuntimeComponent("Val", sizeof(int), alignof(int));

    std::vector<Entity> initialRaw;
    for (int i = 0; i < 8; ++i) {
        Entity e = registry.create();
        *static_cast<int*>(registry.addRaw(e, valId)) = i + 1;
        initialRaw.push_back(e);
    }

    // add-during-each in eachRaw: force multiple RawPool reallocations without UAF or visiting new ones
    int visitedRawCount = 0;
    int sumRaw = 0;
    const ComponentTypeId rawTypes[1] = {valId};
    registry.eachRaw(rawTypes, 1, [&](Entity, void** ptrs) {
        const int v = *static_cast<int*>(ptrs[0]);
        visitedRawCount++;
        sumRaw += v;
        for (int k = 0; k < 10; ++k) {
            Entity spawned = registry.create();
            *static_cast<int*>(registry.addRaw(spawned, valId)) = 1000;
        }
    });
    CHECK(visitedRawCount == 8);
    CHECK(sumRaw == 36);

    // destroy-during-each in eachRaw: destroying the next pending entity skips it safely
    int visitedAfterDestroy = 0;
    registry.eachRaw(rawTypes, 1, [&](Entity e, void**) {
        visitedAfterDestroy++;
        if (e == initialRaw[0]) {
            registry.destroy(initialRaw[1]);
        }
        registry.destroy(e);
    });
    CHECK(visitedAfterDestroy < 88);
    CHECK_FALSE(registry.valid(initialRaw[1]));

    // Same safety for View<T>::each with add and destroy during iteration
    Registry typedReg;
    std::vector<Entity> typedEnts;
    for (int i = 0; i < 6; ++i) {
        Entity e = typedReg.create();
        typedReg.add<Position>(e, Position{static_cast<float>(i), 0.0f});
        typedEnts.push_back(e);
    }
    int visitedTyped = 0;
    typedReg.each<Position>([&](Entity e, Position& p) {
        const float origX = p.x;
        visitedTyped++;
        if (e == typedEnts[0]) {
            typedReg.destroy(typedEnts[1]);
        }
        for (int k = 0; k < 16; ++k) {
            Entity spawned = typedReg.create();
            typedReg.add<Position>(spawned, Position{origX + 100.0f, 0.0f});
        }
    });
    CHECK(visitedTyped == 5);
    CHECK_FALSE(typedReg.valid(typedEnts[1]));
}

TEST_CASE("ECS runtime: pointers survive additions (spawner pattern) and eachRaw") {
    struct SpawnerItem {
        int id = 0;
        double value = 0.0;
    };

    Registry registry;
    const ComponentTypeId itemType =
        registry.registerRuntimeComponent("SpawnerItem", sizeof(SpawnerItem), alignof(SpawnerItem));
    REQUIRE(itemType != kInvalidComponentType);

    Entity anchor = registry.create();
    auto* anchorPtr = static_cast<SpawnerItem*>(registry.addRaw(anchor, itemType));
    REQUIRE(anchorPtr != nullptr);
    anchorPtr->id = 77;
    anchorPtr->value = 3.25;
    CHECK(registry.getRaw(anchor, itemType) == anchorPtr);

    // Add 1000 entities with the same component (crosses multiple pages)
    std::vector<Entity> others;
    others.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        Entity e = registry.create();
        auto* p = static_cast<SpawnerItem*>(registry.addRaw(e, itemType));
        REQUIRE(p != nullptr);
        p->id = i + 1;
        p->value = static_cast<double>(i) * 0.5;
        others.push_back(e);
    }

    // Write and read through the old pointer after 1000 additions
    CHECK(anchorPtr->id == 77);
    CHECK(anchorPtr->value == doctest::Approx(3.25));
    anchorPtr->id = 999;
    anchorPtr->value = 12.5;

    auto* fetchedAnchor = static_cast<SpawnerItem*>(registry.getRaw(anchor, itemType));
    CHECK(fetchedAnchor == anchorPtr);
    CHECK(fetchedAnchor->id == 999);
    CHECK(fetchedAnchor->value == doctest::Approx(12.5));

    // Spawner pattern inside eachRaw: read → create entities with the same component → write via old
    // pointer
    const ComponentTypeId types[1] = {itemType};
    int visited = 0;
    registry.eachRaw(types, 1, [&](Entity e, void** ptrs) {
        visited++;
        auto* item = static_cast<SpawnerItem*>(ptrs[0]);
        const int origId = item->id;
        if (e == anchor || origId <= 5) {
            for (int k = 0; k < 200; ++k) {
                Entity spawned = registry.create();
                auto* sp = static_cast<SpawnerItem*>(registry.addRaw(spawned, itemType));
                sp->id = 50000 + k;
            }
        }
        item->id = origId * 10;
    });
    CHECK(visited == 1001);
    CHECK(anchorPtr->id == 9990);
    CHECK(static_cast<SpawnerItem*>(registry.getRaw(others[0], itemType))->id == 10);
}

TEST_CASE("ECS runtime: 64-byte alignment across page boundaries and after 1000 additions") {
    Registry registry;
    const ComponentTypeId align64Type = registry.registerRuntimeComponent("Align64", 40, 64);
    REQUIRE(align64Type != kInvalidComponentType);

    std::vector<Entity> entities;
    std::vector<void*> ptrs;
    entities.reserve(1000);
    ptrs.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        Entity e = registry.create();
        void* p = registry.addRaw(e, align64Type);
        REQUIRE(p != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(p) % 64 == 0);
        *static_cast<int*>(p) = i + 100;
        entities.push_back(e);
        ptrs.push_back(p);
    }

    // Verify key indices (first page, 63/64/65 boundary, and rest) and intact contents
    CHECK(reinterpret_cast<std::uintptr_t>(ptrs[0]) % 64 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(ptrs[63]) % 64 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(ptrs[64]) % 64 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(ptrs[65]) % 64 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(ptrs[999]) % 64 == 0);

    for (int i = 0; i < 1000; ++i) {
        CHECK(registry.getRaw(entities[static_cast<std::size_t>(i)], align64Type) ==
              ptrs[static_cast<std::size_t>(i)]);
        CHECK(*static_cast<int*>(ptrs[static_cast<std::size_t>(i)]) == i + 100);
    }
}

TEST_CASE("ECS runtime: swap-and-pop preserves values of remaining entities") {
    struct Entry {
        int index;
        std::uint64_t magic;
    };

    Registry registry;
    const ComponentTypeId entryType =
        registry.registerRuntimeComponent("Entry", sizeof(Entry), alignof(Entry));
    REQUIRE(entryType != kInvalidComponentType);

    constexpr int kTotal = 150;
    std::vector<Entity> entities;
    std::vector<Entry*> ptrs;
    entities.reserve(kTotal);
    ptrs.reserve(kTotal);

    for (int i = 0; i < kTotal; ++i) {
        Entity e = registry.create();
        auto* p = static_cast<Entry*>(registry.addRaw(e, entryType));
        p->index = i;
        p->magic = 0xCAFE0000ULL + static_cast<std::uint64_t>(i);
        entities.push_back(e);
        ptrs.push_back(p);
    }

    // Remove a middle entity in the first page and another in the second page
    CHECK(registry.removeRaw(entities[1], entryType));
    CHECK_FALSE(registry.hasRaw(entities[1], entryType));
    registry.destroy(entities[65]);
    CHECK_FALSE(registry.hasRaw(entities[65], entryType));

    // Pointers to unmoved and unremoved elements (e.g. 0, 10, 64) remain valid
    CHECK(registry.getRaw(entities[0], entryType) == ptrs[0]);
    CHECK(registry.getRaw(entities[10], entryType) == ptrs[10]);
    CHECK(registry.getRaw(entities[64], entryType) == ptrs[64]);
    ptrs[10]->index = 1010;

    // All remaining entities keep their values
    for (int i = 0; i < kTotal; ++i) {
        if (i == 1 || i == 65) continue;
        REQUIRE(registry.hasRaw(entities[static_cast<std::size_t>(i)], entryType));
        const auto* got =
            static_cast<const Entry*>(registry.getRaw(entities[static_cast<std::size_t>(i)], entryType));
        const int expectedIndex = (i == 10) ? 1010 : i;
        CHECK(got->index == expectedIndex);
        CHECK(got->magic == 0xCAFE0000ULL + static_cast<std::uint64_t>(i));
    }
}

TEST_CASE("ECS runtime: RawPool move constructor and move assignment") {
    ecs_detail::RawPool poolA(sizeof(int), 64);
    std::vector<Entity> entities;
    for (std::uint32_t i = 0; i < 130; ++i) {
        Entity e{i, 0};
        void* p = poolA.emplace(e);
        *static_cast<int*>(p) = static_cast<int>(i * 3 + 1);
        entities.push_back(e);
    }
    void* ptr0 = poolA.get(entities[0].index);
    void* ptr70 = poolA.get(entities[70].index);

    ecs_detail::RawPool poolB(std::move(poolA));
    CHECK(poolA.size() == 0);
    CHECK(poolB.size() == 130);
    CHECK(poolB.get(entities[0].index) == ptr0);
    CHECK(poolB.get(entities[70].index) == ptr70);
    CHECK(*static_cast<int*>(poolB.get(entities[70].index)) == 211);

    ecs_detail::RawPool poolC(sizeof(int), 64);
    Entity temp{999, 0};
    *static_cast<int*>(poolC.emplace(temp)) = 42;
    poolC = std::move(poolB);
    CHECK(poolB.size() == 0);
    CHECK(poolC.size() == 130);
    CHECK(poolC.get(entities[0].index) == ptr0);
    CHECK(*static_cast<int*>(poolC.get(entities[0].index)) == 1);
    CHECK(*static_cast<int*>(poolC.get(entities[129].index)) == 388);
}
