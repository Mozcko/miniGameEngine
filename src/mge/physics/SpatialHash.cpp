#include <mge/physics/SpatialHash.h>
#include <mge/core/Log.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mge {

    namespace {

        // Función hash para empaquetar una coordenada 3D entera en 64 bits (igual a ChunkCoordHash)
        inline std::uint64_t cellKey(int x, int y, int z) {
            std::uint64_t h = static_cast<std::uint32_t>(x);
            h = h * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint32_t>(y);
            h = h * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint32_t>(z);
            return h ^ (h >> 31);
        }

        // Conversión segura de coordenada flotante a índice de celda (evita UB en float -> int)
        inline int cellCoord(float v, float cellSize) {
            if (std::isnan(v)) {
                return 0;
            }
            const float c = std::floor(v / cellSize);
            if (c <= static_cast<float>(std::numeric_limits<int>::min() + 10)) {
                return std::numeric_limits<int>::min() + 10;
            }
            if (c >= static_cast<float>(std::numeric_limits<int>::max() - 10)) {
                return std::numeric_limits<int>::max() - 10;
            }
            return static_cast<int>(c);
        }

    } // namespace

    SpatialHash::SpatialHash(float initialCellSize) : m_cellSize(initialCellSize) {
        MGE_ASSERT(initialCellSize > 0.0f, "SpatialHash: cellSize debe ser mayor a 0");
    }

    void SpatialHash::clear() {
        m_entries.clear();
        m_cellItems.clear();
        m_sorted = true;
    }

    void SpatialHash::insert(std::uint32_t id, const Aabb& box) {
        const std::uint32_t entryIndex = static_cast<std::uint32_t>(m_entries.size());
        m_entries.push_back({id, box});
        m_sorted = false;

        if (box.isEmpty()) {
            return;
        }

        const int minX = cellCoord(box.min.x, m_cellSize);
        const int minY = cellCoord(box.min.y, m_cellSize);
        const int minZ = cellCoord(box.min.z, m_cellSize);

        const int maxX = cellCoord(box.max.x, m_cellSize);
        const int maxY = cellCoord(box.max.y, m_cellSize);
        const int maxZ = cellCoord(box.max.z, m_cellSize);

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    m_cellItems.push_back({cellKey(x, y, z), entryIndex});
                }
            }
        }
    }

    void SpatialHash::prepare() const {
        if (!m_sorted) {
            std::sort(m_cellItems.begin(), m_cellItems.end());
            m_sorted = true;
        }
    }

    void SpatialHash::computePairs(std::vector<std::pair<std::uint32_t, std::uint32_t>>& outPairs) const {
        outPairs.clear();
        if (m_entries.size() < 2 || m_cellItems.empty()) {
            return;
        }
        prepare();

        std::size_t i = 0;
        while (i < m_cellItems.size()) {
            std::size_t j = i + 1;
            while (j < m_cellItems.size() && m_cellItems[j].cellKey == m_cellItems[i].cellKey) {
                ++j;
            }

            // Comparar todos los pares de entradas que coinciden en esta celda
            for (std::size_t a = i; a < j; ++a) {
                const std::uint32_t idxA = m_cellItems[a].entryIndex;
                const Entry& entryA = m_entries[idxA];
                for (std::size_t b = a + 1; b < j; ++b) {
                    const std::uint32_t idxB = m_cellItems[b].entryIndex;
                    if (idxA == idxB) {
                        continue;
                    }
                    const Entry& entryB = m_entries[idxB];
                    if (entryA.id == entryB.id) {
                        continue;
                    }
                    if (entryA.box.intersects(entryB.box)) {
                        const std::uint32_t minId = std::min(entryA.id, entryB.id);
                        const std::uint32_t maxId = std::max(entryA.id, entryB.id);
                        outPairs.emplace_back(minId, maxId);
                    }
                }
            }
            i = j;
        }

        std::sort(outPairs.begin(), outPairs.end());
        outPairs.erase(std::unique(outPairs.begin(), outPairs.end()), outPairs.end());
    }

    void SpatialHash::query(const Aabb& box, std::vector<std::uint32_t>& outIds) const {
        outIds.clear();
        if (box.isEmpty() || m_entries.empty() || m_cellItems.empty()) {
            return;
        }
        prepare();

        const int minX = cellCoord(box.min.x, m_cellSize);
        const int minY = cellCoord(box.min.y, m_cellSize);
        const int minZ = cellCoord(box.min.z, m_cellSize);

        const int maxX = cellCoord(box.max.x, m_cellSize);
        const int maxY = cellCoord(box.max.y, m_cellSize);
        const int maxZ = cellCoord(box.max.z, m_cellSize);

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    const std::uint64_t key = cellKey(x, y, z);
                    auto it = std::lower_bound(m_cellItems.begin(), m_cellItems.end(), key,
                                               [](const CellItem& item, std::uint64_t targetKey) {
                                                   return item.cellKey < targetKey;
                                               });
                    while (it != m_cellItems.end() && it->cellKey == key) {
                        const Entry& entry = m_entries[it->entryIndex];
                        if (entry.box.intersects(box)) {
                            outIds.push_back(entry.id);
                        }
                        ++it;
                    }
                }
            }
        }

        std::sort(outIds.begin(), outIds.end());
        outIds.erase(std::unique(outIds.begin(), outIds.end()), outIds.end());
    }

    std::size_t SpatialHash::size() const {
        return m_entries.size();
    }

    float SpatialHash::cellSize() const {
        return m_cellSize;
    }

} // namespace mge
