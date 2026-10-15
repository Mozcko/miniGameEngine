#pragma once

#include <mge/core/ecs/Registry.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace mge {

    // En qué parte del frame corre un sistema
    enum class SystemPhase {
        Fixed,  // Dentro de cada paso fijo (simulación determinista), dt = fixedDelta
        Update, // Una vez por frame antes de onUpdate, dt = delta escalado
        Render, // Una vez por frame antes de onRender, dt = delta escalado
        Count
    };

    class World;
    using SystemFunction = std::function<void(World& world, float dt)>;

    // Registro de entidades + sistemas ordenados por fase. La Application tiene uno (app.world())
    // y corre cada fase en su loop; los sistemas de una fase corren en el orden en que se agregaron.
    class World {
    public:
        Registry& registry() { return m_registry; }
        const Registry& registry() const { return m_registry; }

        void addSystem(SystemPhase phase, std::string name, SystemFunction system);
        void run(SystemPhase phase, float dt);
        std::size_t systemCount(SystemPhase phase) const;

    private:
        struct System {
            std::string name;
            SystemFunction function;
        };

        Registry m_registry;
        std::array<std::vector<System>, static_cast<std::size_t>(SystemPhase::Count)> m_systems;
    };
}
