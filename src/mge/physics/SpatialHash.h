#pragma once

#include <mge/core/math/Geometry.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mge {

    // Broadphase: uniform 3D cells. Rebuilt every step (clear + insert); stores no pointers.
    // Boxes spanning many cells (e.g. > 8 per axis) work correctly by inserting into every cell
    // they touch (O(dx * dy * dz)), which is more expensive but safe for any size.
    class SpatialHash {
    public:
        explicit SpatialHash(float initialCellSize = 2.0f); // initialCellSize > 0 (MGE_ASSERT)

        // Preserves reserved memory (does not free vector capacities)
        void clear();

        void insert(std::uint32_t id, const Aabb& box);

        // Pairs (a, b) with a < b whose boxes touch (Aabb::intersects), WITHOUT duplicates even when
        // sharing multiple cells, sorted by (a, b): deterministic and independent of internal hash order.
        void computePairs(std::vector<std::pair<std::uint32_t, std::uint32_t>>& outPairs) const;

        // IDs whose boxes touch `box`, sorted and deduplicated
        void query(const Aabb& box, std::vector<std::uint32_t>& outIds) const;

        std::size_t size() const; // Number of inserted boxes
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
