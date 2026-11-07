#pragma once

#include <mge/voxel/Block.h>
#include <mge/voxel/ChunkMesher.h>
#include <mge/voxel/ChunkSnapshot.h>

#include <vector>

namespace mge {

    // Representación submuestreada de un chunk en factor f = 2^lod (con dimensión dim = 16/f bloques por
    // eje). Cada celda o "super-bloque" representa un volumen de f³ bloques del chunk original.
    struct LodChunk {
        int factor = 1;              // Factor de submuestreo f = 2^lod (1, 2, 4...)
        std::vector<BlockId> blocks; // (16/f)³ bloques
        int dimension = 16;          // 16 / factor (ej. 8 para LOD 1, 4 para LOD 2)

        LodChunk() = default;
        LodChunk(int f, int dim)
            : factor(f), blocks(static_cast<std::size_t>(dim * dim * dim), kAir), dimension(dim) {}

        BlockId get(int x, int y, int z) const {
            if (x < 0 || x >= dimension || y < 0 || y >= dimension || z < 0 || z >= dimension) {
                return kAir;
            }
            return blocks[static_cast<std::size_t>((y * dimension + z) * dimension + x)];
        }
        BlockId get(const glm::ivec3& p) const { return get(p.x, p.y, p.z); }

        void set(int x, int y, int z, BlockId id) {
            if (x >= 0 && x < dimension && y >= 0 && y < dimension && z >= 0 && z < dimension) {
                blocks[static_cast<std::size_t>((y * dimension + z) * dimension + x)] = id;
            }
        }
    };

    // Submuestreo de un chunk según la regla de visibilidad cenital:
    // Para cada celda de f³ bloques se escoge el bloque que vería un observador desde arriba:
    // se escanea la celda de arriba hacia abajo (en el eje Y, desde la capa superior hacia la inferior)
    // y se toma el primer bloque OPACO que no sea aire. Si en toda la celda no hay ningún bloque opaco,
    // se devuelve aire (kAir). Los bloques transparentes o recortados (cutout) se descartan a LOD > 0.
    LodChunk downsample(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int lod);
    LodChunk downsample(const ChunkSnapshot& snapshot, int lod);

    // Construye la malla LOD de un chunk en coordenadas locales (0..16).
    // - LOD 0: devuelve la malla completa normal generada por buildChunkMesh.
    // - LOD n (1, 2): submuestrea el chunk y el borde de sus vecinos a factor f = 2^lod,
    //   emitiendo únicamente la malla opaca (ChunkMesh::opaque).
    // - Caras vecinas: utiliza el borde del snapshot submuestreado de la misma forma, por lo que las
    //   caras entre dos chunks del MISMO LOD se ocultan exactamente igual que en LOD 0.
    // - Grietas entre distintos LODs: agrega faldas (skirts) en los 4 bordes laterales (±X, ±Z),
    //   extruyendo las caras exteriores de los super-bloques del borde hacia abajo f bloques
    //   (quads verticales del mismo color) si ChunkMeshOptions::lodSkirts es true (por defecto).
    ChunkMesh buildLodChunkMesh(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int lod,
                                const ChunkMeshOptions& options = {});

} // namespace mge
