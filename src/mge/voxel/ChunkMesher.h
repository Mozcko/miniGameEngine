#pragma once

#include <mge/procgen/MeshData.h>
#include <mge/voxel/VoxelWorld.h>

namespace mge {

    struct ChunkMeshOptions {
        TextureAtlas atlas; // Para los bloques con tiles; los de solo color tienen UV (0, 0)
    };

    // Malla de un chunk en coordenadas locales (0..16): dibujar con model = translate(chunkOrigin).
    // opaque: bloques opacos. transparent: el resto (agua, vidrio, hojas, cruces).
    struct ChunkMesh {
        MeshData opaque;
        MeshData transparent;
    };

    // Construye la malla de un chunk cargado (vacía si no lo está). Solo emite caras visibles:
    // una cara se oculta si el vecino es opaco, o si ambos son el mismo bloque transparente
    // (agua contra agua). Los vecinos se leen del mundo: un chunk vecino sin cargar cuenta como aire.
    ChunkMesh buildChunkMesh(const VoxelWorld& world, const ChunkCoord& coord,
                             const ChunkMeshOptions& options = {});
}
