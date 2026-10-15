#pragma once

#include <mge/voxel/Block.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

namespace mge {

    // Chunks cúbicos de 16³ bloques, apilados también en vertical.
    // Convención: el bloque (x, y, z) ocupa el cubo [x, x+1) × [y, y+1) × [z, z+1) del mundo.
    inline constexpr int kChunkSize = 16;
    inline constexpr int kChunkVolume = kChunkSize * kChunkSize * kChunkSize;

    struct ChunkCoord {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const ChunkCoord& o) const { return !(*this == o); }
        bool operator<(const ChunkCoord& o) const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            return z < o.z;
        }
    };

    struct ChunkCoordHash {
        std::size_t operator()(const ChunkCoord& c) const {
            std::uint64_t h = static_cast<std::uint32_t>(c.x);
            h = h * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint32_t>(c.y);
            h = h * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint32_t>(c.z);
            return static_cast<std::size_t>(h ^ (h >> 31));
        }
    };

    // --- Coordenadas: mundo → chunk → local (correctas también con negativos) ---
    // División y módulo hacia -infinito: floorDiv(-1, 16) = -1, floorMod(-1, 16) = 15
    constexpr int floorDiv(int a, int b) {
        return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0);
    }
    constexpr int floorMod(int a, int b) {
        return a - floorDiv(a, b) * b;
    }

    inline ChunkCoord chunkOf(const glm::ivec3& block) {
        return {floorDiv(block.x, kChunkSize), floorDiv(block.y, kChunkSize), floorDiv(block.z, kChunkSize)};
    }
    inline glm::ivec3 localOf(const glm::ivec3& block) {
        return {floorMod(block.x, kChunkSize), floorMod(block.y, kChunkSize), floorMod(block.z, kChunkSize)};
    }
    // Primer bloque (esquina mínima) del chunk en coordenadas de mundo
    inline glm::ivec3 chunkOrigin(const ChunkCoord& c) {
        return {c.x * kChunkSize, c.y * kChunkSize, c.z * kChunkSize};
    }
    // Bloque que contiene un punto del mundo
    inline glm::ivec3 blockAt(const glm::vec3& p) {
        return glm::ivec3(glm::floor(p));
    }

    // Bloques de un chunk (8 KB). Índice: x varía más rápido, luego z, luego y.
    class ChunkData {
    public:
        static constexpr int index(int x, int y, int z) { return (y * kChunkSize + z) * kChunkSize + x; }
        static constexpr bool inside(int x, int y, int z) {
            return x >= 0 && x < kChunkSize && y >= 0 && y < kChunkSize && z >= 0 && z < kChunkSize;
        }

        // Coordenadas locales (0..15); fuera de rango → aire (get) o se ignora (set)
        BlockId get(int x, int y, int z) const { return inside(x, y, z) ? m_blocks[index(x, y, z)] : kAir; }
        BlockId get(const glm::ivec3& local) const { return get(local.x, local.y, local.z); }
        void set(int x, int y, int z, BlockId id) {
            if (inside(x, y, z)) m_blocks[index(x, y, z)] = id;
        }
        void set(const glm::ivec3& local, BlockId id) { set(local.x, local.y, local.z, id); }

        void fill(BlockId id) { m_blocks.fill(id); }
        bool isEmpty() const; // Solo aire
        const std::array<BlockId, kChunkVolume>& blocks() const { return m_blocks; }

    private:
        std::array<BlockId, kChunkVolume> m_blocks{}; // Todo aire
    };
}
