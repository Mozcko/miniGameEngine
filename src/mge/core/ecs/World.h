#pragma once

#include <mge/core/ecs/Registry.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace mge {

    // When in the frame a system runs
    enum class SystemPhase {
        Fixed,  // Inside each fixed step (deterministic simulation), dt = fixedDelta
        Update, // Once per frame before onUpdate, dt = scaled delta
        Render, // Once per frame before onRender, dt = scaled delta
        Count
    };

    class World;
    class Profiler;
    using SystemFunction = std::function<void(World& world, float dt)>;

    // Entity registry + systems ordered by phase. Application owns one (app.world())
    // and runs each phase in its loop; systems within a phase run in registration order.
    class World {
    public:
        Registry& registry() { return m_registry; }
        const Registry& registry() const { return m_registry; }

        void addSystem(SystemPhase phase, std::string name, SystemFunction system);
        void run(SystemPhase phase, float dt);
        std::size_t systemCount(SystemPhase phase) const;

        void setProfiler(Profiler* profiler);
        Profiler* profiler() const { return m_profiler; }

    private:
        struct System {
            std::string name;
            SystemFunction function;
        };

        Registry m_registry;
        std::array<std::vector<System>, static_cast<std::size_t>(SystemPhase::Count)> m_systems;
        Profiler* m_profiler = nullptr;
    };
}
