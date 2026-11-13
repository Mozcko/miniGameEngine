#include <mge/core/ecs/Registry.h>

#include <mge/core/Log.h>

namespace mge {

    namespace {
        constexpr std::size_t kMaxRuntimeComponentSize = 1024u * 1024u;
        constexpr std::size_t kMaxRuntimeComponentAlign = 4096u;
    } // namespace

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
        if (name.empty()) {
            MGE_LOG_ERROR("Ecs", "registerRuntimeComponent: el nombre del componente no puede estar vacío");
            return kInvalidComponentType;
        }
        const std::size_t actualAlign = align == 0 ? alignof(std::max_align_t) : align;
        if ((actualAlign & (actualAlign - 1)) != 0 || actualAlign > kMaxRuntimeComponentAlign) {
            MGE_LOG_ERROR("Ecs", "registerRuntimeComponent('", name, "'): alineación inválida ", align,
                          " (debe ser potencia de 2 y <= ", kMaxRuntimeComponentAlign, ")");
            return kInvalidComponentType;
        }
        if (size >= kMaxRuntimeComponentSize) {
            MGE_LOG_ERROR("Ecs", "registerRuntimeComponent('", name, "'): tamaño inválido ", size,
                          " (debe ser < ", kMaxRuntimeComponentSize, ")");
            return kInvalidComponentType;
        }

        auto it = m_runtimeComponentNames.find(name);
        if (it != m_runtimeComponentNames.end()) {
            const ComponentTypeId existingId = it->second;
            const auto& existingPool = m_rawPools[existingId];
            if (existingPool->elementSize() == size && existingPool->elementAlign() == actualAlign) {
                return existingId;
            }
            MGE_LOG_ERROR("Ecs", "registerRuntimeComponent('", name,
                          "'): conflicto de layout (registrado size=", existingPool->elementSize(),
                          " align=", existingPool->elementAlign(), ", solicitado size=", size,
                          " align=", actualAlign, ")");
            return kInvalidComponentType;
        }
        const ComponentTypeId id = m_rawPools.size();
        m_rawPools.push_back(std::make_unique<ecs_detail::RawPool>(size, actualAlign));
        m_runtimeComponents.push_back(RuntimeComponentInfo{name, size, actualAlign});
        m_runtimeComponentNames.emplace(std::move(name), id);
        return id;
    }

    void* Registry::addRaw(Entity entity, ComponentTypeId type) {
        if (!valid(entity) || !isValidRuntimeComponent(type)) {
            return nullptr;
        }
        return m_rawPools[type]->emplace(entity);
    }

    void* Registry::getRaw(Entity entity, ComponentTypeId type) {
        if (!hasRaw(entity, type)) {
            return nullptr;
        }
        return m_rawPools[type]->get(entity.index);
    }

    const void* Registry::getRaw(Entity entity, ComponentTypeId type) const {
        if (!hasRaw(entity, type)) {
            return nullptr;
        }
        return m_rawPools[type]->get(entity.index);
    }

    bool Registry::hasRaw(Entity entity, ComponentTypeId type) const {
        if (!valid(entity) || !isValidRuntimeComponent(type)) return false;
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
            if (!isValidRuntimeComponent(id)) return;
            const auto* p = m_rawPools[id].get();
            if (!smallest || p->size() < smallest->size()) {
                smallest = p;
            }
        }
        if (!smallest || smallest->size() == 0) return;

        // Copiar entidades antes de iterar para tolerar altas/bajas estructurales durante el callback
        const std::vector<Entity> entities = smallest->entities();
        std::vector<void*> ptrs(typeCount);
        for (Entity entity : entities) {
            if (!valid(entity)) {
                continue;
            }
            bool hasAll = true;
            for (std::size_t i = 0; i < typeCount; ++i) {
                if (!m_rawPools[types[i]]->contains(entity.index)) {
                    hasAll = false;
                    break;
                }
            }
            if (!hasAll) {
                continue;
            }
            for (std::size_t i = 0; i < typeCount; ++i) {
                ptrs[i] = m_rawPools[types[i]]->get(entity.index);
            }
            callback(entity, ptrs.data());
        }
    }
}
