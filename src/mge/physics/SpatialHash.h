#pragma once

#include <mge/core/math/Geometry.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mge {

    // Broadphase: celdas uniformes en 3D. Se reconstruye cada paso (clear + insert); no guarda punteros.
    // Cajas que abarcan muchas celdas (ej: > 8 por eje) funcionan correctamente insertándose en todas las
    // celdas que tocan (O(dx * dy * dz)), siendo más costoso pero seguro ante cualquier tamaño.
    class SpatialHash {
    public:
        explicit SpatialHash(float initialCellSize = 2.0f); // initialCellSize > 0 (MGE_ASSERT)

        // Conserva la memoria reservada (no libera capacidad de vectores)
        void clear();

        void insert(std::uint32_t id, const Aabb& box);

        // Pares (a, b) con a < b cuyas cajas se tocan (Aabb::intersects), SIN duplicados aunque compartan
        // varias celdas, ordenados por (a, b): determinista e independiente del orden interno del hash.
        void computePairs(std::vector<std::pair<std::uint32_t, std::uint32_t>>& outPairs) const;

        // Ids cuyas cajas tocan `box`, ordenados y sin duplicados
        void query(const Aabb& box, std::vector<std::uint32_t>& outIds) const;

        std::size_t size() const; // Cantidad de cajas insertadas
        float cellSize() const;

    private:
        struct Entry {
            std::uint32_t id = 0;
            Aabb box;
        };

        struct CellItem {
            std::uint64_t cellKey = 0;
            std::uint32_t entryIndex = 0;

            bool operator<(const CellItem& other) const {
                if (cellKey != other.cellKey) {
                    return cellKey < other.cellKey;
                }
                return entryIndex < other.entryIndex;
            }
        };

        void prepare() const;

        float m_cellSize = 2.0f;
        std::vector<Entry> m_entries;
        mutable std::vector<CellItem> m_cellItems;
        mutable bool m_sorted = true;
    };

} // namespace mge
