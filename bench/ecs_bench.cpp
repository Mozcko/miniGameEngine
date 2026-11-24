// ECS benchmark (not a test): creates N entities and iterates typical systems.
// Usage: ./bench/ecs_bench [N]   (default 100000). Build in Release for useful numbers.
#include <mge/core/ecs/Registry.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
    struct Position {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    struct Velocity {
        float x = 1.0f, y = 0.5f, z = 0.25f;
    };
    struct Health {
        int value = 100;
    };

    template <typename Fn> double measureMs(Fn&& fn) {
        auto start = std::chrono::steady_clock::now();
        fn();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
}

int main(int argc, char** argv) {
    const int count = argc > 1 ? std::atoi(argv[1]) : 100000;
    mge::Registry registry;

    double createMs = measureMs([&] {
        for (int i = 0; i < count; i++) {
            mge::Entity e = registry.create();
            registry.add<Position>(e);
            registry.add<Velocity>(e);
            if (i % 2 == 0) registry.add<Health>(e); // Only half
        }
    });

    const int iterations = 100;
    double sum = 0.0;
    double iterateMs = measureMs([&] {
        for (int it = 0; it < iterations; it++) {
            registry.each<Position, Velocity>([](mge::Entity, Position& p, Velocity& v) {
                p.x += v.x * 0.016f;
                p.y += v.y * 0.016f;
                p.z += v.z * 0.016f;
            });
        }
    });
    double sparseMs = measureMs([&] {
        for (int it = 0; it < iterations; it++) {
            registry.each<Health, Position>(
                [&](mge::Entity, Health& h, Position& p) { sum += h.value + p.x; });
        }
    });

    std::printf("ECS benchmark (%d entities)\n", count);
    std::printf("  create + 2.5 components avg      : %8.2f ms (%6.1f ns/entity)\n", createMs,
                createMs * 1e6 / count);
    std::printf("  each<Position, Velocity>         : %8.3f ms/iter (%5.2f ns/entity)\n",
                iterateMs / iterations, iterateMs * 1e6 / iterations / count);
    std::printf("  each<Health, Position> (50%%)     : %8.3f ms/iter (%5.2f ns/entity)\n",
                sparseMs / iterations, sparseMs * 1e6 / iterations / (count / 2));
    std::printf("  (checksum %.1f)\n", sum);
    return 0;
}
