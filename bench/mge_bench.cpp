#include "Bench.h"

#include <mge/core/Jobs.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/PhysicsWorld.h>
#include <mge/physics/RigidBody.h>
#include <mge/physics/SpatialHash.h>
#include <mge/physics/VoxelCollision.h>
#include <mge/physics/VoxelRaycast.h>
#include <mge/procgen/Noise.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/ChunkMesher.h>
#include <mge/voxel/ChunkSnapshot.h>
#include <mge/voxel/ChunkStreamer.h>
#include <mge/voxel/VoxelWorld.h>

#if defined(MGE_BENCH_HAS_GPU)
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuNoiseField.h>
#include <SDL3/SDL.h>
#endif

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using namespace mge;
using namespace mge::bench;

namespace {

    // --- 1) Noise benchmarks ---
    void registerNoiseBenchmarks(BenchRunner& runner) {
        const int count = runner.isQuick() ? 100000 : 1000000;
        const Noise noise(1337);
        FractalSettings fractal;

        runner.add("noise: perlin 2D", "ns/muestra", count, [=] {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i) * 0.0137f;
                sum += noise.perlin(x, x * 0.71f + 3.1f);
            }
            doNotOptimize(sum);
        });

        runner.add("noise: simplex 2D", "ns/muestra", count, [=] {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i) * 0.0137f;
                sum += noise.simplex(x, x * 0.71f + 3.1f);
            }
            doNotOptimize(sum);
        });

        runner.add("noise: perlin 3D", "ns/muestra", count, [=] {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i) * 0.0137f;
                sum += noise.perlin(x, x * 0.71f + 3.1f, x * 0.37f - 1.9f);
            }
            doNotOptimize(sum);
        });

        runner.add("noise: simplex 3D", "ns/muestra", count, [=] {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i) * 0.0137f;
                sum += noise.simplex(x, x * 0.71f + 3.1f, x * 0.37f - 1.9f);
            }
            doNotOptimize(sum);
        });

        runner.add("noise: fbm simplex 2D", "ns/muestra", count, [=] {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const float x = static_cast<float>(i) * 0.0137f;
                sum += noise.fbm(NoiseType::Simplex, x, x * 0.71f + 3.1f, fractal);
            }
            doNotOptimize(sum);
        });

        // CPU fbm 64³ vs GPU fbm 64³
        constexpr int kDim = 64;
        constexpr int kTotalSamples = kDim * kDim * kDim;
        const glm::vec3 origin(0.0f);
        const glm::vec3 scale(0.05f);

        runner.add("noise: CPU fbm 64^3", "ns/campo", 1, [=] {
            std::vector<float> data(static_cast<std::size_t>(kTotalSamples));
            for (int z = 0; z < kDim; ++z) {
                for (int y = 0; y < kDim; ++y) {
                    for (int x = 0; x < kDim; ++x) {
                        const glm::vec3 pos = origin + glm::vec3(x, y, z) * scale;
                        data[static_cast<std::size_t>(x + y * kDim + z * (kDim * kDim))] =
                            noise.fbm(NoiseType::Simplex, pos.x, pos.y, pos.z, fractal);
                    }
                }
            }
            doNotOptimize(data.data());
        });

#if defined(MGE_BENCH_HAS_GPU)
        if (!std::getenv("SDL_VIDEODRIVER")) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            GpuDeviceOptions options;
            options.debug = false;
            std::unique_ptr<GpuDevice> devicePtr = GpuDevice::create(options);
            if (devicePtr) {
                auto device = std::shared_ptr<GpuDevice>(std::move(devicePtr));
                auto field = std::make_shared<GpuNoiseField>(*device, noise);
                GpuNoiseFieldDesc desc;
                desc.width = kDim;
                desc.height = kDim;
                desc.depth = kDim;
                desc.origin = origin;
                desc.scale = scale;
                desc.fractal = fractal;

                runner.add("noise: GPU fbm 64^3 incl. download", "ns/campo", 1, [device, field, desc] {
                    field->generate(desc);
                    const std::vector<float> data = field->download();
                    doNotOptimize(data.data());
                });
            }
        }
#endif
    }

    // --- 2) ECS benchmarks ---
    struct Position {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    struct Velocity {
        float x = 1.0f, y = 0.5f, z = 0.25f;
    };

    void registerEcsBenchmarks(BenchRunner& runner) {
        const int count = runner.isQuick() ? 10000 : 100000;
        auto registry = std::make_shared<Registry>();
        for (int i = 0; i < count; ++i) {
            Entity e = registry->create();
            registry->add<Position>(e, Position{float(i), 0.0f, 0.0f});
            registry->add<Velocity>(e, Velocity{1.0f, 0.5f, 0.25f});
        }

        runner.add("ecs: iterate 100k entities with 2 components", "ns/entidad", count, [registry] {
            registry->each<Position, Velocity>([](Entity, Position& p, Velocity& v) {
                p.x += v.x * 0.016f;
                p.y += v.y * 0.016f;
                p.z += v.z * 0.016f;
            });
            doNotOptimize(*registry);
        });
    }

    // --- 3) Voxel benchmarks ---
    class BenchTerrainGenerator : public TerrainGenerator {
    public:
        explicit BenchTerrainGenerator(BlockId stone, BlockId air = 0, std::uint32_t seed = 1337)
            : m_stone(stone), m_air(air), m_noise(seed) {}

        void generate(const ChunkCoord& coord, ChunkData& chunk) const override {
            const glm::ivec3 origin = chunkOrigin(coord);
            for (int z = 0; z < kChunkSize; ++z) {
                for (int x = 0; x < kChunkSize; ++x) {
                    const int wx = origin.x + x;
                    const int wz = origin.z + z;
                    const float h = m_noise.simplex(wx * 0.02f, wz * 0.02f) * 16.0f + 16.0f;
                    const int height = static_cast<int>(std::floor(h));
                    for (int y = 0; y < kChunkSize; ++y) {
                        const int wy = origin.y + y;
                        if (wy <= height) {
                            const float cave = m_noise.simplex(wx * 0.05f, wy * 0.05f, wz * 0.05f);
                            if (cave > 0.3f && wy < height) {
                                chunk.set(x, y, z, m_air);
                            } else {
                                chunk.set(x, y, z, m_stone);
                            }
                        } else {
                            chunk.set(x, y, z, m_air);
                        }
                    }
                }
            }
        }

    private:
        BlockId m_stone;
        BlockId m_air;
        Noise m_noise;
    };

    void convergeStreamer(ChunkStreamer& streamer, const ChunkCoord& center) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            streamer.update(center);
            if (streamer.idle()) {
                return;
            }
            const StreamingStats stats = streamer.stats();
            if (stats.pendingGenerate + stats.pendingMesh > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void registerVoxelBenchmarks(BenchRunner& runner) {
        auto registry = std::make_shared<BlockRegistry>();
        BlockDesc stoneDesc;
        stoneDesc.name = "stone";
        stoneDesc.shape = BlockShape::Cube;
        stoneDesc.layer = RenderLayer::Opaque;
        stoneDesc.color = {0.6f, 0.6f, 0.6f, 1.0f};
        const BlockId stoneId = registry->add(stoneDesc);

        auto gen = std::make_shared<BenchTerrainGenerator>(stoneId);

        // a) generate 16^3 chunk
        runner.add("voxel: generate chunk 16^3", "ns/voxel", kChunkVolume, [gen] {
            ChunkData chunk;
            gen->generate({0, 0, 0}, chunk);
            doNotOptimize(chunk);
        });

        // Setup world with generated chunks to capture snapshot
        auto world =
            std::make_shared<VoxelWorld>(*registry, std::make_unique<BenchTerrainGenerator>(stoneId));
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    world->loadChunk({dx, dy, dz});
                }
            }
        }
        const ChunkSnapshot snapshot = ChunkSnapshot::capture(*world, {0, 0, 0});

        // b) mesh chunk with buildChunkMesh (greedy on/off, AO on/off)
        runner.add("voxel: mesh chunk (greedy=off, AO=off)", "ns/mesh", 1, [=] {
            ChunkMeshOptions opt;
            opt.greedy = false;
            opt.ambientOcclusion = false;
            auto mesh = buildChunkMesh(snapshot, *registry, opt);
            doNotOptimize(mesh);
        });

        runner.add("voxel: mesh chunk (greedy=on, AO=off)", "ns/mesh", 1, [=] {
            ChunkMeshOptions opt;
            opt.greedy = true;
            opt.ambientOcclusion = false;
            auto mesh = buildChunkMesh(snapshot, *registry, opt);
            doNotOptimize(mesh);
        });

        runner.add("voxel: mesh chunk (greedy=off, AO=on)", "ns/mesh", 1, [=] {
            ChunkMeshOptions opt;
            opt.greedy = false;
            opt.ambientOcclusion = true;
            auto mesh = buildChunkMesh(snapshot, *registry, opt);
            doNotOptimize(mesh);
        });

        runner.add("voxel: mesh chunk (greedy=on, AO=on)", "ns/mesh", 1, [=] {
            ChunkMeshOptions opt;
            opt.greedy = true;
            opt.ambientOcclusion = true;
            auto mesh = buildChunkMesh(snapshot, *registry, opt);
            doNotOptimize(mesh);
        });

        // c) ChunkStreamer load 8-chunk radius (sync vs async JobSystem)
        const int radius = runner.isQuick() ? 3 : 8;
        const std::uint64_t chunkCount = static_cast<std::uint64_t>((2 * radius + 1) * (2 * radius + 1));

        runner.add("voxel: streamer load 8-radius sync", "ns/chunk", chunkCount, [=] {
            VoxelWorld streamWorld(*registry, std::make_unique<BenchTerrainGenerator>(stoneId));
            StreamingSettings settings;
            settings.loadRadius = radius;
            settings.meshRadius = radius - 1;
            settings.unloadMargin = 2;
            settings.minChunkY = 0;
            settings.maxChunkY = 0;
            settings.maxResultsPerUpdate = 512;
            ChunkStreamer streamer(streamWorld, settings, nullptr);
            convergeStreamer(streamer, {0, 0, 0});
            doNotOptimize(streamWorld);
        });

        runner.add("voxel: streamer load 8-radius async (JobSystem)", "ns/chunk", chunkCount, [=] {
            VoxelWorld streamWorld(*registry, std::make_unique<BenchTerrainGenerator>(stoneId));
            StreamingSettings settings;
            settings.loadRadius = radius;
            settings.meshRadius = radius - 1;
            settings.unloadMargin = 2;
            settings.minChunkY = 0;
            settings.maxChunkY = 0;
            settings.maxJobsInFlight = 32;
            settings.maxResultsPerUpdate = 512;
            JobSystem jobs;
            ChunkStreamer streamer(streamWorld, settings, &jobs);
            convergeStreamer(streamer, {0, 0, 0});
            doNotOptimize(streamWorld);
        });
    }

    // --- 4) Physics benchmarks ---
    void registerPhysicsBenchmarks(BenchRunner& runner) {
        // a) moveAabb 10k calls on a heightmap
        const int moveCalls = runner.isQuick() ? 1000 : 10000;
        const SolidQuery heightmap = [](const glm::ivec3& p) { return p.y <= 10 + (p.x % 3); };

        runner.add("physics: moveAabb 10k calls on heightmap", "ns/call", moveCalls, [=] {
            MoveResult lastRes;
            for (int i = 0; i < moveCalls; ++i) {
                const Aabb box{
                    glm::vec3(static_cast<float>(i % 100), 12.0f, static_cast<float>((i / 100) % 100)),
                    glm::vec3(0.45f)};
                const glm::vec3 delta(0.2f, -2.5f, 0.1f);
                lastRes = moveAabb(box, delta, heightmap);
            }
            doNotOptimize(lastRes);
        });

        // b) raycastVoxels 10k rays
        const int rayCount = runner.isQuick() ? 1000 : 10000;
        auto rayReg = std::make_shared<BlockRegistry>();
        BlockDesc stone;
        stone.name = "stone";
        stone.shape = BlockShape::Cube;
        const BlockId stoneId = rayReg->add(stone);
        auto rayWorld = std::make_shared<VoxelWorld>(*rayReg);
        // Put a solid layer from y = -5 to y = 5
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                ChunkData& c = rayWorld->loadChunk({x, 0, z});
                for (int ly = 0; ly < 6; ++ly) {
                    for (int lz = 0; lz < kChunkSize; ++lz) {
                        for (int lx = 0; lx < kChunkSize; ++lx) {
                            c.set(lx, ly, lz, stoneId);
                        }
                    }
                }
            }
        }

        runner.add("physics: raycastVoxels 10k rays", "ns/ray", rayCount, [rayReg, rayWorld, rayCount] {
            std::optional<VoxelRaycastHit> lastHit;
            for (int i = 0; i < rayCount; ++i) {
                const float ox = static_cast<float>((i * 7) % 30) - 15.0f;
                const float oz = static_cast<float>((i * 13) % 30) - 15.0f;
                const Ray ray{glm::vec3(ox, 25.0f, oz), glm::vec3(0.0f, -1.0f, 0.0f)};
                lastHit = raycastVoxels(*rayWorld, ray, 50.0f);
            }
            doNotOptimize(lastHit);
        });

        // c) PhysicsWorld::step with 500 bodies on a floor
        const int bodyCount = runner.isQuick() ? 50 : 500;
        const SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };

        runner.add("physics: PhysicsWorld::step 500 bodies on floor", "ns/cuerpo", bodyCount, [=] {
            PhysicsWorld world({}, floorQuery);
            Registry reg;
            for (int i = 0; i < bodyCount; ++i) {
                Entity e = reg.create();
                const float x = static_cast<float>(i % 25) * 1.5f - 18.0f;
                const float z = static_cast<float>(i / 25) * 1.5f - 15.0f;
                const float y = 2.0f + static_cast<float>(i % 5) * 1.0f;
                reg.add<Transform>(e, Transform{glm::vec3(x, y, z)});
                RigidBody rb;
                rb.shape = ShapeType::Box;
                rb.halfExtents = glm::vec3(0.45f);
                rb.mass = 1.0f;
                reg.add<RigidBody>(e, rb);
            }
            world.step(reg, 1.0f / 60.0f);
            doNotOptimize(reg);
        });

        // d) SpatialHash computePairs with 2000 boxes
        const int boxCount = runner.isQuick() ? 200 : 2000;
        auto hash = std::make_shared<SpatialHash>(2.0f);
        for (int i = 0; i < boxCount; ++i) {
            const float x = static_cast<float>(i % 40) * 1.5f;
            const float y = static_cast<float>((i / 40) % 10) * 1.5f;
            const float z = static_cast<float>(i / 400) * 1.5f;
            hash->insert(static_cast<std::uint32_t>(i), Aabb{glm::vec3(x, y, z), glm::vec3(1.0f)});
        }

        runner.add("physics: SpatialHash computePairs 2000 boxes", "ns/caja", boxCount, [=] {
            std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
            hash->computePairs(pairs);
            doNotOptimize(pairs);
        });
    }

} // namespace

int main(int argc, char** argv) {
    BenchRunner runner(argc, argv);

    registerNoiseBenchmarks(runner);
    registerEcsBenchmarks(runner);
    registerVoxelBenchmarks(runner);
    registerPhysicsBenchmarks(runner);

    const int repeats = runner.isQuick() ? 3 : 7;
    runner.run(repeats);
    runner.printTable();

    if (!runner.jsonPath().empty()) {
        runner.writeJson(runner.jsonPath());
    }

    return 0;
}
