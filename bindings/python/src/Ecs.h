#pragma once

#include "Common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mge::python {

    inline uint64_t entityToId(const mge::Entity& e) {
        if (e.isNull()) return 0xFFFFFFFFFFFFFFFFULL;
        return (static_cast<uint64_t>(e.generation) << 32) | static_cast<uint64_t>(e.index);
    }

    inline mge::Entity idToEntity(uint64_t id) {
        if (id == 0xFFFFFFFFFFFFFFFFULL) return mge::kNullEntity;
        return mge::Entity{static_cast<std::uint32_t>(id & 0xFFFFFFFFu),
                           static_cast<std::uint32_t>(id >> 32)};
    }

    struct PyEntity {
        uint64_t id = 0xFFFFFFFFFFFFFFFFULL;

        PyEntity() = default;
        explicit PyEntity(uint64_t entityId) : id(entityId) {}
        explicit PyEntity(const mge::Entity& e) : id(entityToId(e)) {}

        uint32_t index() const { return static_cast<uint32_t>(id & 0xFFFFFFFFu); }
        uint32_t generation() const { return static_cast<uint32_t>(id >> 32); }
        bool isNull() const { return id == 0xFFFFFFFFFFFFFFFFULL; }
        mge::Entity toEngine() const { return idToEntity(id); }

        bool operator==(const PyEntity& other) const { return id == other.id; }
        bool operator!=(const PyEntity& other) const { return id != other.id; }
    };

    class PyWorld {
    public:
        PyWorld();
        explicit PyWorld(mge::World* world);
        ~PyWorld();

        mge::World& engineWorld() { return *m_worldRef; }
        const mge::World& engineWorld() const { return *m_worldRef; }
        mge::Registry& registry() { return m_worldRef->registry(); }
        const mge::Registry& registry() const { return m_worldRef->registry(); }

        PyEntity create();
        void destroy(PyEntity entity);
        bool isAlive(PyEntity entity) const;

        void add(PyEntity entity, nanobind::object comp);
        bool has(PyEntity entity, nanobind::object compType) const;
        nanobind::object get(PyEntity entity, nanobind::object compType);
        void remove(PyEntity entity, nanobind::object compType);

        nanobind::object each(nanobind::args args);
        void addSystem(nanobind::object phase, const std::string& name, nanobind::object fn);
        void clear();

    private:
        std::unique_ptr<mge::World> m_ownedWorld;
        mge::World* m_worldRef = nullptr;
        std::unordered_map<uint64_t, std::unordered_map<PyTypeObject*, nanobind::object>> m_pyComponents;
        std::vector<std::shared_ptr<nanobind::object>> m_pySystems;
    };

} // namespace mge::python
