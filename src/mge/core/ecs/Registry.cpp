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
        for (auto& rawPool : m_rawPools) {
            if (rawPool) rawPool->remove(entity.index);
        }
        // La generación nueva invalida todos los handles viejos de este índice
        m_generations[entity.index]++;
        m_freeIndices.push_back(entity.index);
        m_alive--;
    }

    bool Registry::valid(Entity entity) const {
        return entity.index < m_generations.size() && m_generations[entity.index] == entity.generation;
    }

    ComponentTypeId Registry::registerRuntimeComponent(std::string name, std::size_t size,
                                                       std::size_t align) {
        auto it = m_runtimeComponentNames.find(name);
        if (it != m_runtimeComponentNames.end()) {
            return it->second;
        }
        const ComponentTypeId id = m_rawPools.size();
        m_rawPools.push_back(std::make_unique<ecs_detail::RawPool>(size, align));
        m_runtimeComponents.push_back(RuntimeComponentInfo{name, size, align});
        m_runtimeComponentNames.emplace(std::move(name), id);
        return id;
    }

    void* Registry::addRaw(Entity entity, ComponentTypeId type) {
        MGE_ASSERT(valid(entity), "addRaw() sobre una entidad inválida");
        MGE_ASSERT(type < m_rawPools.size(), "addRaw() con ComponentTypeId desconocido");
        return m_rawPools[type]->emplace(entity);
    }

    void* Registry::getRaw(Entity entity, ComponentTypeId type) {
        MGE_ASSERT(hasRaw(entity, type), "getRaw() de un componente que la entidad no tiene");
        return m_rawPools[type]->get(entity.index);
    }

    const void* Registry::getRaw(Entity entity, ComponentTypeId type) const {
        MGE_ASSERT(hasRaw(entity, type), "getRaw() const de un componente que la entidad no tiene");
        return m_rawPools[type]->get(entity.index);
    }

    bool Registry::hasRaw(Entity entity, ComponentTypeId type) const {
        if (!valid(entity) || type >= m_rawPools.size() || !m_rawPools[type]) return false;
        return m_rawPools[type]->contains(entity.index);
    }

    bool Registry::removeRaw(Entity entity, ComponentTypeId type) {
        if (!hasRaw(entity, type)) return false;
        m_rawPools[type]->remove(entity.index);
        return true;
    }

    void Registry::eachRaw(const ComponentTypeId* types, std::size_t typeCount,
                           const std::function<void(Entity, void**)>& callback) {
        if (typeCount == 0 || !types || !callback) return;

        const ecs_detail::RawPool* smallest = nullptr;
        for (std::size_t i = 0; i < typeCount; ++i) {
            const ComponentTypeId id = types[i];
            if (id >= m_rawPools.size() || !m_rawPools[id]) return;
            const auto* p = m_rawPools[id].get();
            if (!smallest || p->size() < smallest->size()) {
                smallest = p;
            }
        }
        if (!smallest || smallest->size() == 0) return;

        std::vector<void*> ptrs(typeCount);
        for (Entity entity : smallest->entities()) {
            bool hasAll = true;
            for (std::size_t i = 0; i < typeCount; ++i) {
                auto* p = m_rawPools[types[i]].get();
                if (!p->contains(entity.index)) {
                    hasAll = false;
                    break;
                }
                ptrs[i] = p->get(entity.index);
            }
            if (hasAll) {
                callback(entity, ptrs.data());
            }
        }
    }
}
