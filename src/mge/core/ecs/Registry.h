#pragma once

#include <mge/core/Log.h>
#include <mge/core/ecs/Entity.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <tuple>
#include <unordered_map>
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

        // Almacenamiento "sparse set" de tipo borrado para componentes en tiempo de ejecución.
        // Mantiene memoria alineada en páginas fijas (kPageSize) que nunca se mueven ni liberan
        // hasta destruir el pool: los punteros sobreviven a altas (addRaw), mientras que las bajas
        // usan swap-and-pop (invalidan el puntero quitado y el del último elemento movido al hueco).
        class RawPool : public PoolBase {
        public:
            static constexpr std::size_t kPageSize = 64;

            RawPool(std::size_t elementSize, std::size_t elementAlign)
                : m_elementSize(elementSize),
                  m_elementAlign(elementAlign > 0 ? elementAlign : alignof(std::max_align_t)),
                  m_stride(calculateStride(elementSize, m_elementAlign)) {}

            ~RawPool() override { deallocate(); }

            RawPool(const RawPool&) = delete;
            RawPool& operator=(const RawPool&) = delete;

            RawPool(RawPool&& other) noexcept
                : m_elementSize(other.m_elementSize), m_elementAlign(other.m_elementAlign),
                  m_stride(other.m_stride), m_pages(std::move(other.m_pages)) {
                m_sparse = std::move(other.m_sparse);
                m_dense = std::move(other.m_dense);
                other.m_pages.clear();
            }

            RawPool& operator=(RawPool&& other) noexcept {
                if (this != &other) {
                    deallocate();
                    m_sparse = std::move(other.m_sparse);
                    m_dense = std::move(other.m_dense);
                    m_elementSize = other.m_elementSize;
                    m_elementAlign = other.m_elementAlign;
                    m_stride = other.m_stride;
                    m_pages = std::move(other.m_pages);
                    other.m_pages.clear();
                }
                return *this;
            }

            void* emplace(Entity entity) {
                if (contains(entity.index)) {
                    void* ptr = slotPtr(m_sparse[entity.index]);
                    if (m_elementSize > 0) std::memset(ptr, 0, m_elementSize);
                    return ptr;
                }
                if (entity.index >= m_sparse.size()) {
                    m_sparse.resize(entity.index + 1, kAbsent);
                }
                const std::size_t pos = m_dense.size();
                const std::size_t pageIndex = pos / kPageSize;
                if (pageIndex >= m_pages.size()) {
                    // Crecimiento geométrico antes de operator new para evitar realojamientos O(n^2) y fugas
                    if (m_pages.size() == m_pages.capacity()) {
                        m_pages.reserve(m_pages.empty() ? 8 : m_pages.capacity() * 2);
                    }
                    auto* page = static_cast<std::byte*>(
                        ::operator new(kPageSize * m_stride, std::align_val_t{m_elementAlign}));
                    m_pages.push_back(page);
                }
                m_dense.push_back(entity);
                m_sparse[entity.index] = static_cast<std::uint32_t>(pos);
                void* ptr = slotPtr(pos);
                if (m_elementSize > 0) std::memset(ptr, 0, m_elementSize);
                return ptr;
            }

            void remove(std::uint32_t index) override {
                if (!contains(index)) return;
                const std::uint32_t position = m_sparse[index];
                const std::uint32_t last = static_cast<std::uint32_t>(m_dense.size() - 1);
                if (position != last) {
                    m_dense[position] = m_dense[last];
                    if (m_elementSize > 0) {
                        std::memcpy(slotPtr(position), slotPtr(last), m_elementSize);
                    }
                    m_sparse[m_dense[position].index] = position;
                }
                m_dense.pop_back();
                m_sparse[index] = kAbsent;
            }

            void* get(std::uint32_t index) { return slotPtr(m_sparse[index]); }

            const void* get(std::uint32_t index) const { return slotPtr(m_sparse[index]); }

            // Retorna nullptr si no existe; evita doble búsqueda en m_sparse durante eachRaw.
            void* tryGetRaw(std::uint32_t index) {
                if (index >= m_sparse.size()) return nullptr;
                const std::uint32_t pos = m_sparse[index];
                return pos != kAbsent ? slotPtr(pos) : nullptr;
            }

            std::size_t elementSize() const { return m_elementSize; }
            std::size_t elementAlign() const { return m_elementAlign; }
            std::size_t stride() const { return m_stride; }

        private:
            static std::size_t calculateStride(std::size_t size, std::size_t align) {
                const std::size_t aligned = (size + align - 1) & ~(align - 1);
                return std::max<std::size_t>(aligned > 0 ? aligned : align, 1);
            }

            std::byte* slotPtr(std::size_t denseIndex) const {
                return m_pages[denseIndex / kPageSize] + (denseIndex % kPageSize) * m_stride;
            }

            void deallocate() {
                for (std::byte* page : m_pages) {
                    ::operator delete(page, std::align_val_t{m_elementAlign});
                }
                m_pages.clear();
            }

            std::size_t m_elementSize = 0;
            std::size_t m_elementAlign = 1;
            std::size_t m_stride = 0;
            std::vector<std::byte*> m_pages;
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

    using ComponentTypeId = std::size_t;
    inline constexpr ComponentTypeId kInvalidComponentType = std::numeric_limits<ComponentTypeId>::max();

    struct RuntimeComponentInfo {
        std::string name;
        std::size_t size = 0;
        std::size_t align = 1;
    };

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

        // --- Componentes en tiempo de ejecución (C, Python, etc.) ---
        // Los punteros devueltos por addRaw(), getRaw() y eachRaw() permanecen válidos al agregar
        // componentes (las páginas nunca se reubican). Quitar un componente (removeRaw) o destruir
        // una entidad (destroy) hace swap-and-pop e invalida los punteros a ese componente y al que
        // era el último en su pool.
        bool isValidRuntimeComponent(ComponentTypeId type) const {
            return type < m_rawPools.size() && m_rawPools[type] != nullptr;
        }
        ComponentTypeId registerRuntimeComponent(std::string name, std::size_t size, std::size_t align);
        void* addRaw(Entity entity, ComponentTypeId type);
        void* getRaw(Entity entity, ComponentTypeId type);
        const void* getRaw(Entity entity, ComponentTypeId type) const;
        bool hasRaw(Entity entity, ComponentTypeId type) const;
        bool removeRaw(Entity entity, ComponentTypeId type);

        void eachRaw(const ComponentTypeId* types, std::size_t typeCount,
                     const std::function<void(Entity, void**)>& callback);

        template <typename Container, typename Fn> void eachRaw(const Container& types, Fn&& callback) {
            eachRaw(types.data(), types.size(), std::forward<Fn>(callback));
        }

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
        std::vector<std::unique_ptr<ecs_detail::PoolBase>> m_pools;   // Por typeId
        std::vector<std::unique_ptr<ecs_detail::RawPool>> m_rawPools; // Por ComponentTypeId
        std::unordered_map<std::string, ComponentTypeId> m_runtimeComponentNames;
        std::vector<RuntimeComponentInfo> m_runtimeComponents;
    };

    // Vista sobre las entidades que tienen todos los componentes Ts. Recorre el pool más chico
    // y verifica los demás, así el costo es proporcional al componente menos común.
    template <typename... Ts> class View {
    public:
        explicit View(Registry& registry) : m_registry(registry) {}

        template <typename Fn> void each(Fn&& fn) {
            const ecs_detail::PoolBase* smallest = smallestPool();
            if (!smallest || smallest->size() == 0) return; // Algún tipo no tiene ningún componente todavía
            // Copiar la lista de entidades: el callback puede agregar/quitar componentes o destruir
            // entidades sin invalidar el iterador ni visitar entidades creadas durante este pase.
            const std::vector<Entity> entities = smallest->entities();
            for (Entity entity : entities) {
                if (!m_registry.valid(entity)) {
                    continue;
                }
                // Re-obtener los pools por si un pool se creó o creció durante la iteración
                auto pools = std::make_tuple(m_registry.findPool<Ts>()...);
                if ((std::get<ecs_detail::Pool<Ts>*>(pools) && ...) &&
                    (std::get<ecs_detail::Pool<Ts>*>(pools)->contains(entity.index) && ...)) {
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
