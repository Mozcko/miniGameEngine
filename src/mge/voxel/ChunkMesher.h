#pragma once

#include <mge/procgen/MeshData.h>
#include <mge/voxel/ChunkSnapshot.h>
#include <mge/voxel/VoxelWorld.h>

namespace mge {

    struct ChunkMeshOptions {
        TextureAtlas atlas; // Para los bloques con tiles; los de solo color tienen UV (0, 0)
        // Une caras vecinas iguales en un solo quad (solo bloques de color, sin textura: los tiles de un
        // atlas no se pueden repetir sobre un quad grande hasta el renderer nuevo, M3)
        bool greedy = true;
        // Oclusión ambiental por vértice: oscurece el color en rincones y bordes (0 a 3 vecinos opacos).
        // Con greedy, solo se unen caras con la misma oclusión en sus 4 esquinas.
        bool ambientOcclusion = false;
        float aoStrength = 0.5f; // Color en un rincón cerrado = 1 - aoStrength
        // Bloques con textura: false (por defecto) = color de vértice blanco (la textura tal cual, más el
        // AO); true = la textura se tiñe con BlockDesc::color. Los bloques sin textura usan siempre su color.
        bool tintTextured = false;
    };

    // Malla de un chunk en coordenadas locales (0..16): dibujar con model = translate(chunkOrigin).
    // Una malla por pasada de dibujo (BlockDesc::renderLayer): opaque (sólidos), cutout (hojas, plantas:
    // prueba alfa) y transparent (translúcidos: agua, vidrio, lava; mezcla alfa).
    struct ChunkMesh {
        MeshData opaque;
        MeshData cutout;
        MeshData transparent;
    };

    // Construye la malla de un chunk cargado (vacía si no lo está). Solo emite caras visibles:
    // una cara se oculta si el vecino es un cubo opaco, o si ambos son el mismo bloque transparente
    // (agua contra agua). Los vecinos se leen del mundo, también los de otros chunks; un chunk vecino
    // sin cargar cuenta como aire (VoxelWorld marca sucio al chunk cuando ese vecino se carga).
    ChunkMesh buildChunkMesh(const VoxelWorld& world, const ChunkCoord& coord,
                             const ChunkMeshOptions& options = {});
    // Igual, desde una copia (ChunkSnapshot::capture). Solo lee el snapshot y el registry (que no debe
    // cambiar mientras tanto): se puede llamar desde otro hilo (JobSystem).
    ChunkMesh buildChunkMesh(const ChunkSnapshot& snapshot, const BlockRegistry& registry,
                             const ChunkMeshOptions& options = {});
}
