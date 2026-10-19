#pragma once

#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace mge {

    // Llena un chunk recién creado. Debe ser determinista (mismo coord → mismos bloques) y no depender
    // de qué otros chunks estén cargados: así el mundo no cambia según el orden de carga.
    class TerrainGenerator {
    public:
        virtual ~TerrainGenerator() = default;
        virtual void generate(const ChunkCoord& coord, ChunkData& chunk) const = 0;
    };

    // Mundo voxel: chunks cargados (sin límite de tamaño, también en vertical), acceso por coordenadas
    // de mundo y marcas de "sucio" para saber qué chunks hay que volver a mallar.
    class VoxelWorld {
    public:
        // El registry debe vivir más que el mundo. Sin generador, los chunks nuevos quedan vacíos.
        explicit VoxelWorld(const BlockRegistry& registry,
                            std::unique_ptr<TerrainGenerator> generator = nullptr);

        const BlockRegistry& registry() const { return m_registry; }
        // El generador (nullptr si no hay). generate() es const y no debe tener estado: se puede llamar
        // desde otros hilos para generar chunks en paralelo e insertarlos después con insertChunk.
        const TerrainGenerator* generator() const { return m_generator.get(); }

        // Crea y genera el chunk si no existe; si ya existe lo devuelve tal cual. El chunk nuevo queda
        // sucio, y también sus 6 vecinos cargados: su malla veía aire en ese lado y ahora hay datos.
        ChunkData& loadChunk(const ChunkCoord& coord);
        // Agrega un chunk ya generado (ej: en un JobSystem). Si ya existía, se conserva el existente.
        // Igual que loadChunk: queda sucio y ensucia a sus vecinos cargados.
        ChunkData& insertChunk(const ChunkCoord& coord, const ChunkData& data);
        void unloadChunk(const ChunkCoord& coord);
        bool isLoaded(const ChunkCoord& coord) const { return m_chunks.count(coord) != 0; }
        ChunkData* chunk(const ChunkCoord& coord);
        const ChunkData* chunk(const ChunkCoord& coord) const;
        std::size_t chunkCount() const { return m_chunks.size(); }
        std::vector<ChunkCoord> loadedChunks() const;

        // Coordenadas de bloque del mundo. Chunk sin cargar → aire.
        BlockId getBlock(const glm::ivec3& block) const;
        BlockId getBlock(int x, int y, int z) const { return getBlock({x, y, z}); }
        // false si el chunk no está cargado. Marca sucio el chunk y, si el bloque está en un borde,
        // también el vecino de ese lado (su malla mira este bloque).
        bool setBlock(const glm::ivec3& block, BlockId id);
        bool isSolid(const glm::ivec3& block) const { return m_registry.isSolid(getBlock(block)); }

        // --- Sucios: chunks cuyo contenido (o el de un vecino en el borde) cambió ---
        bool isDirty(const ChunkCoord& coord) const;
        void markDirty(const ChunkCoord& coord); // Ignora chunks no cargados
        void clearDirty(const ChunkCoord& coord);
        std::vector<ChunkCoord> dirtyChunks() const;

    private:
        struct Entry {
            ChunkData data;
            bool dirty = true;
        };

        void markFaceNeighboursDirty(const ChunkCoord& coord);

        const BlockRegistry& m_registry;
        std::unique_ptr<TerrainGenerator> m_generator;
        // unique_ptr: direcciones estables aunque el mapa crezca
        std::unordered_map<ChunkCoord, std::unique_ptr<Entry>, ChunkCoordHash> m_chunks;
    };
}
