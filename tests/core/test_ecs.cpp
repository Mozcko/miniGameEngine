#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>

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
        std::string name; // Componente no trivial (se mueve en el swap-and-pop)
    };
}

TEST_CASE("Entidades: crear, destruir, reciclar índices e invalidar handles viejos") {
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

    Entity c = registry.create(); // Reutiliza el índice de a con otra generación
    CHECK(c.index == a.index);
    CHECK(c.generation != a.generation);
    CHECK(registry.valid(c));
    CHECK_FALSE(registry.valid(a)); // El handle viejo sigue inválido

    registry.destroy(a); // Destruir un handle inválido no hace nada
    CHECK(registry.valid(c));
    CHECK_FALSE(registry.valid(kNullEntity));
}

TEST_CASE("Componentes: add, get, has, tryGet, remove, reemplazo") {
    Registry registry;
    Entity e = registry.create();
    CHECK_FALSE(registry.has<Position>(e));
    CHECK(registry.tryGet<Position>(e) == nullptr);

    registry.add<Position>(e, Position{1.0f, 2.0f});
    CHECK(registry.has<Position>(e));
    CHECK(registry.get<Position>(e).y == doctest::Approx(2.0f));
    registry.get<Position>(e).x = 5.0f;
    CHECK(registry.tryGet<Position>(e)->x == doctest::Approx(5.0f));

    registry.add<Position>(e, Position{9.0f, 9.0f}); // Reemplaza
    CHECK(registry.count<Position>() == 1);
    CHECK(registry.get<Position>(e).x == doctest::Approx(9.0f));

    registry.remove<Position>(e);
    CHECK_FALSE(registry.has<Position>(e));
    CHECK(registry.count<Position>() == 0);
    registry.remove<Position>(e); // Quitar algo que no está: no pasa nada
}

TEST_CASE("Destruir una entidad quita todos sus componentes y no rompe los demás (swap-and-pop)") {
    Registry registry;
    std::vector<Entity> entities;
    for (int i = 0; i < 5; i++) {
        Entity e = registry.create();
        registry.add<Named>(e, Named{"e" + std::to_string(i)});
        registry.add<Position>(e, Position{static_cast<float>(i), 0.0f});
        entities.push_back(e);
    }
    registry.destroy(entities[1]); // Del medio: el último ocupa su lugar
    CHECK(registry.count<Named>() == 4);
    CHECK(registry.get<Named>(entities[4]).name == "e4");
    CHECK(registry.get<Position>(entities[4]).x == doctest::Approx(4.0f));
    CHECK(registry.get<Named>(entities[0]).name == "e0");

    // Una entidad nueva en el índice reciclado no hereda componentes
    Entity recycled = registry.create();
    CHECK(recycled.index == entities[1].index);
    CHECK_FALSE(registry.has<Named>(recycled));
}

TEST_CASE("view<A, B> recorre solo las entidades con ambos componentes") {
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
        p.x += v.dx; // Se puede modificar dentro del each
    });
    CHECK(visited == withBoth);
    CHECK(registry.view<Position, Velocity>().count() == 4);
    CHECK(registry.view<Position, Tag>().count() == 1);
    CHECK(registry.view<Position, Named>().count() == 0); // Tipo sin pool todavía
}

TEST_CASE("TransformSystem: matrices de mundo con jerarquía (padres antes que hijos)") {
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
    // El nieto hereda la escala 2 del hijo: su (1,0,0) local queda a 2 unidades
    CHECK(origin(grandchild).x == doctest::Approx(12.0f));
    CHECK(origin(grandchild).y == doctest::Approx(5.0f));
}

TEST_CASE("TransformSystem: rotación, padre destruido y ciclos") {
    Registry registry;
    Entity pivot = registry.create();
    Transform rotated;
    rotated.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    registry.add<Transform>(pivot, rotated);
    Entity arm = registry.create();
    registry.add<Transform>(arm, Transform{{1.0f, 0.0f, 0.0f}});
    registry.add<Parent>(arm, Parent{pivot});

    // Ciclo a <-> b: no debe colgarse
    Entity a = registry.create();
    Entity b = registry.create();
    registry.add<Transform>(a);
    registry.add<Transform>(b);
    registry.add<Parent>(a, Parent{b});
    registry.add<Parent>(b, Parent{a});

    Log::setSink([](const LogRecord&) {}); // Silenciar el aviso del ciclo
    TransformSystem::update(registry);
    Log::setSink(nullptr);

    glm::vec3 armWorld = glm::vec3(registry.get<WorldTransform>(arm).matrix * glm::vec4(0, 0, 0, 1));
    CHECK(armWorld.x == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(armWorld.z == doctest::Approx(-1.0f)); // +X girado 90° sobre Y -> -Z
    CHECK(registry.has<WorldTransform>(a));

    // Padre destruido: el hijo pasa a ser raíz
    registry.destroy(pivot);
    TransformSystem::update(registry);
    armWorld = glm::vec3(registry.get<WorldTransform>(arm).matrix * glm::vec4(0, 0, 0, 1));
    CHECK(armWorld.x == doctest::Approx(1.0f));
}

TEST_CASE("World: los sistemas corren por fase y en orden de registro") {
    World world;
    std::vector<std::string> calls;
    world.addSystem(SystemPhase::Update, "a", [&](World&, float) { calls.push_back("update.a"); });
    world.addSystem(SystemPhase::Fixed, "physics", [&](World&, float) { calls.push_back("fixed"); });
    world.addSystem(SystemPhase::Update, "b", [&](World&, float) { calls.push_back("update.b"); });
    world.run(SystemPhase::Update, 0.016f);
    world.run(SystemPhase::Render, 0.016f); // Sin sistemas
    std::vector<std::string> expected = {"update.a", "update.b"};
    CHECK(calls == expected);
    CHECK(world.systemCount(SystemPhase::Fixed) == 1);
}

TEST_CASE("Una app headless corre los sistemas del World en sus fases durante N frames") {
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
            // Movimiento en el paso fijo: determinista
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
    // 60 pasos fijos de 1/60 s a 1 u/s = 1 unidad
    app.world().registry().each<Position>([](Entity, Position& p) { CHECK(p.x == doctest::Approx(1.0f)); });
}

TEST_CASE("ECS runtime: alineación estricta a 16 y 32 bytes") {
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

TEST_CASE("ECS runtime: operaciones addRaw, getRaw, hasRaw, removeRaw") {
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

    // addRaw inicializa la memoria en cero
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

TEST_CASE("ECS runtime: eachRaw con múltiples componentes") {
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

TEST_CASE("ECS runtime: destruir entidad limpia los componentes raw") {
    Registry registry;
    const ComponentTypeId typeId = registry.registerRuntimeComponent("Test", sizeof(double), alignof(double));

    Entity e = registry.create();
    registry.addRaw(e, typeId);
    CHECK(registry.hasRaw(e, typeId));

    registry.destroy(e);
    CHECK_FALSE(registry.hasRaw(e, typeId));
}

TEST_CASE("ECS runtime: registrar muchos tipos de componentes") {
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
