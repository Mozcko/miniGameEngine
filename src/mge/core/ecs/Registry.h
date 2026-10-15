#pragma once

#include <mge/core/Log.h>
#include <mge/core/ecs/Entity.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace mge {

    namespace ecs_detail {
        // Almacenamiento "sparse set" de un tipo de componente:
        //   sparse[entity.index] -> posición en los arreglos densos (o kAbsent)
        //   dense[i] = entidad dueña, data[i] = componente
        // Iterar es recorrer arreglos contiguos; agregar/quitar/buscar es O(1).
        class PoolBase {
        public:
            virtual ~PoolBase() = default;
            virtual void remove(std::uint32_t index) = 0;
            bool contains(std::uint32_t index) const {
                return index < m_sparse.size() && m_sparse[index] != kAbsent;
            }
            std::size_t size() const { return m_dense.size(); }
            const std::vector<Entity>& entities() const { return m_dense; }

        protected:
            static constexpr std::uint32_t kAbsent = 0xFFFFFFFFu;
            std::vector<std::uint32_t> m_sparse;
            std::vector<Entity> m_dense;
        };

        template <typename T> class Pool : public PoolBase {
        public:
            template <typename... Args> T& emplace(Entity entity, Args&&... args) {
                if (contains(entity.index)) {
                    // Reemplazar el componente existente
                    T& existing = m_data[m_sparse[entity.index]];
                    existing = T{std::forward<Args>(args)...};
                    return existing;
                }
                if (entity.index >= m_sparse.size()) m_sparse.resize(entity.index + 1, kAbsent);
                m_sparse[entity.index] = static_cast<std::uint32_t>(m_dense.size());
                m_dense.push_back(entity);
                m_data.push_back(T{std::forward<Args>(args)...});
                return m_data.back();
            }

            void remove(std::uint32_t index) override {
                if (!contains(index)) return;
                // Swap-and-pop: el último ocupa el hueco (los arreglos quedan contiguos)
                const std::uint32_t position = m_sparse[index];
                const std::uint32_t last = static_cast<std::uint32_t>(m_dense.size() - 1);
                if (position != last) {
                    m_dense[position] = m_dense[last];
                    m_data[position] = std::move(m_data[last]);
                    m_sparse[m_dense[position].index] = position;
                }
                m_dense.pop_back();
                m_data.pop_back();
                m_sparse[index] = kAbsent;
            }

            T& get(std::uint32_t index) { return m_data[m_sparse[index]]; }
            const T& get(std::uint32_t index) const { return m_data[m_sparse[index]]; }

        private:
            std::vector<T> m_data;
        };

        // Id único por tipo de componente (0, 1, 2...), asignado la primera vez que se usa el tipo
        inline std::size_t nextTypeId() {
            static std::size_t counter = 0;
            return counter++;
        }
        template <typename T> std::size_t typeId() {
            static const std::size_t kId = nextTypeId();
            return kId;
        }
    }

    template <typename... Ts> class View;

    // Registro de entidades y componentes (ECS liviano). Un componente es cualquier tipo movible.
    // No es thread-safe. No agregar ni quitar componentes de los tipos que se están iterando
    // dentro de un each(): usar una lista diferida.
    class Registry {
    public:
        Entity create();
        // Destruye la entidad y todos sus componentes. Ignora handles inválidos.
        void destroy(Entity entity);
        bool valid(Entity entity) const;
        std::size_t aliveCount() const { return m_alive; }

        template <typename T, typename... Args> T& add(Entity entity, Args&&... args) {
            MGE_ASSERT(valid(entity), "add() sobre una entidad inválida");
            return pool<T>().emplace(entity, std::forward<Args>(args)...);
        }

        template <typename T> bool has(Entity entity) const {
            const auto* p = findPool<T>();
            return valid(entity) && p && p->contains(entity.index);
        }

        template <typename T> T& get(Entity entity) {
            MGE_ASSERT(has<T>(entity), "get() de un componente que la entidad no tiene");
            return pool<T>().get(entity.index);
        }

        // nullptr si la entidad no es válida o no tiene el componente
        template <typename T> T* tryGet(Entity entity) {
            return has<T>(entity) ? &pool<T>().get(entity.index) : nullptr;
        }

        template <typename T> void remove(Entity entity) {
            if (!valid(entity)) return;
            if (auto* p = findPool<T>()) p->remove(entity.index);
        }

        template <typename T> std::size_t count() const {
            const auto* p = findPool<T>();
            return p ? p->size() : 0;
        }

        // Entidades que tienen TODOS los componentes Ts
        template <typename... Ts> View<Ts...> view();

        // Atajo: registry.each<A, B>([](Entity e, A& a, B& b) { ... });
        template <typename... Ts, typename Fn> void each(Fn&& fn);

    private:
        template <typename... Ts> friend class View;

        template <typename T> ecs_detail::Pool<T>& pool() {
            const std::size_t id = ecs_detail::typeId<T>();
            if (id >= m_pools.size()) m_pools.resize(id + 1);
            if (!m_pools[id]) m_pools[id] = std::make_unique<ecs_detail::Pool<T>>();
            return static_cast<ecs_detail::Pool<T>&>(*m_pools[id]);
        }

        template <typename T> const ecs_detail::Pool<T>* findPool() const {
            const std::size_t id = ecs_detail::typeId<T>();
            if (id >= m_pools.size() || !m_pools[id]) return nullptr;
            return static_cast<const ecs_detail::Pool<T>*>(m_pools[id].get());
        }

        template <typename T> ecs_detail::Pool<T>* findPool() {
            return const_cast<ecs_detail::Pool<T>*>(std::as_const(*this).findPool<T>());
        }

        std::vector<std::uint32_t> m_generations; // Generación actual de cada índice
        std::vector<std::uint32_t> m_freeIndices; // Índices libres para reutilizar
        std::size_t m_alive = 0;
        std::vector<std::unique_ptr<ecs_detail::PoolBase>> m_pools; // Por typeId
    };

    // Vista sobre las entidades que tienen todos los componentes Ts. Recorre el pool más chico
    // y verifica los demás, así el costo es proporcional al componente menos común.
    template <typename... Ts> class View {
    public:
        explicit View(Registry& registry) : m_registry(registry) {}

        template <typename Fn> void each(Fn&& fn) {
            const ecs_detail::PoolBase* smallest = smallestPool();
            if (!smallest) return; // Algún tipo no tiene ningún componente todavía
            // El callback puede modificar componentes, pero no agregar/quitar de estos tipos
            // Buscar cada pool una sola vez (no por entidad)
            auto pools = std::make_tuple(m_registry.findPool<Ts>()...);
            for (Entity entity : smallest->entities()) {
                if ((std::get<ecs_detail::Pool<Ts>*>(pools)->contains(entity.index) && ...)) {
                    fn(entity, std::get<ecs_detail::Pool<Ts>*>(pools)->get(entity.index)...);
                }
            }
        }

        std::size_t count() {
            std::size_t n = 0;
            each([&n](Entity, Ts&...) { n++; });
            return n;
        }

    private:
        const ecs_detail::PoolBase* smallestPool() const {
            const ecs_detail::PoolBase* pools[] = {m_registry.findPool<Ts>()...};
            const ecs_detail::PoolBase* best = nullptr;
            for (const auto* p : pools) {
                if (!p) return nullptr;
                if (!best || p->size() < best->size()) best = p;
            }
            return best;
        }

        Registry& m_registry;
    };

    template <typename... Ts> View<Ts...> Registry::view() {
        return View<Ts...>(*this);
    }

    template <typename... Ts, typename Fn> void Registry::each(Fn&& fn) {
        view<Ts...>().each(std::forward<Fn>(fn));
    }
}
