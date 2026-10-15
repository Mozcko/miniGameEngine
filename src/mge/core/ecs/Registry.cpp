#include <mge/core/ecs/Registry.h>

namespace mge {

    Entity Registry::create() {
        Entity entity;
        if (!m_freeIndices.empty()) {
            entity.index = m_freeIndices.back();
            m_freeIndices.pop_back();
        } else {
            entity.index = static_cast<std::uint32_t>(m_generations.size());
            m_generations.push_back(0);
        }
        entity.generation = m_generations[entity.index];
        m_alive++;
        return entity;
    }

    void Registry::destroy(Entity entity) {
        if (!valid(entity)) return;
        for (auto& pool : m_pools) {
            if (pool) pool->remove(entity.index);
        }
        // La generación nueva invalida todos los handles viejos de este índice
        m_generations[entity.index]++;
        m_freeIndices.push_back(entity.index);
        m_alive--;
    }

    bool Registry::valid(Entity entity) const {
        return entity.index < m_generations.size() && m_generations[entity.index] == entity.generation;
    }
}
