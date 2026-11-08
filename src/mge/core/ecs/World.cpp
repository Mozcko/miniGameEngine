#include <mge/core/ecs/World.h>
#include <mge/core/Profiler.h>

namespace mge {

    void World::setProfiler(Profiler* profiler) {
        m_profiler = profiler;
    }

    void World::addSystem(SystemPhase phase, std::string name, SystemFunction system) {
        m_systems[static_cast<std::size_t>(phase)].push_back({std::move(name), std::move(system)});
    }

    void World::run(SystemPhase phase, float dt) {
        for (System& system : m_systems[static_cast<std::size_t>(phase)]) {
            MGE_PROFILE_SCOPE(m_profiler, system.name);
            system.function(*this, dt);
        }
    }

    std::size_t World::systemCount(SystemPhase phase) const {
        return m_systems[static_cast<std::size_t>(phase)].size();
    }
}
