#pragma once

#include <mge/core/Jobs.h>
#include <mge/voxel/ChunkLod.h>
#include <mge/voxel/ChunkMesher.h>
#include <mge/voxel/VoxelWorld.h>

#include <array>
#include <cstddef>
#include <future>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mge {

    struct StreamingSettings {
        int loadRadius = 5;   // Columnas (distancia de Chebyshev en X/Z) cargadas alrededor del centro
        int meshRadius = 4;   // Columnas malladas (< loadRadius: así sus vecinos ya tienen datos)
        int unloadMargin = 2; // Histéresis: se descarga recién a más de loadRadius + unloadMargin
        int minChunkY = 0;    // Rango vertical de chunks (inclusive)
        int maxChunkY = 0;
        int maxJobsInFlight = 32;     // Trabajos (generar + mallar) a la vez en el JobSystem
        int maxResultsPerUpdate = 16; // Generaciones y mallas que se aplican/entregan por update
        // 0 = sin límite. Si hay más chunks cargados, se descargan los más lejanos fuera del radio de malla
        std::size_t maxLoadedChunks = 0;
        ChunkMeshOptions meshOptions;

        // Radios de los anillos de LOD (distancia horizontal de Chebyshev donde termina cada LOD).
        // Por ejemplo {6, 12, 24} -> LOD 0 hasta radio 6, LOD 1 hasta radio 12, LOD 2 hasta radio 24.
        // 0 (o lodRadius[0] == 0) desactiva LOD (comportamiento anterior: todo LOD 0 hasta meshRadius).
        // Nota: la histéresis de LOD 0 puede mantener un chunk en LOD 0 un anillo más lejos que
        // lodRadius[0] (hasta lodRadius[0] + 1) para evitar oscilaciones de re-mallado en la frontera.
        std::array<int, 3> lodRadius = {0, 0, 0};

        bool lodEnabled() const { return lodRadius[0] > 0; }

        int maxMeshRadius() const {
            if (!lodEnabled()) return meshRadius;
            if (lodRadius[2] > 0) return lodRadius[2];
            if (lodRadius[1] > 0) return lodRadius[1];
            return lodRadius[0];
        }
    };

    struct StreamingStats {
        std::size_t loaded = 0;          // Chunks con datos
        std::size_t pendingGenerate = 0; // Trabajos de generación en curso
        std::size_t pendingMesh = 0;     // Trabajos de mallado en curso
        std::size_t pooled = 0;          // Chunks en el pool de VoxelWorld
        std::size_t discarded =
            0; // Resultados descartados por viejos (chunk editado, descargado o cambio de LOD)
        std::size_t cpuBytes() const { return loaded * sizeof(ChunkData); }
    };

    // Malla entregada por el streamer para un chunk
    struct StreamMesh {
        ChunkCoord coord;
        ChunkMesh mesh;
        int lod = 0;
    };

    // Lo que el juego tiene que hacer después de un update
    struct StreamUpdate {
        std::vector<StreamMesh> meshes;   // Subir a la GPU con su nivel de LOD (reemplaza la anterior)
        std::vector<ChunkCoord> unloaded; // Liberar sus mallas de la GPU
    };

    // Carga, malla y descarga chunks alrededor de un punto que se mueve (#57). Con un JobSystem genera y
    // malla en otros hilos (#56): el hilo principal solo inserta resultados y toma snapshots, con un límite
    // por update, así cruzar a un chunk nuevo no congela el frame. Sin JobSystem hace lo mismo en el hilo
    // principal (tests, programas headless), con el mismo límite por update.
    //   - Lo más cercano primero; la memoria queda acotada por el radio de descarga (y maxLoadedChunks).
    //   - Resultados viejos (chunk descargado, editado después de pedir la malla, o con LOD desactualizado)
    //   se descartan.
    // El VoxelWorld (y su registry) deben vivir más que el streamer; la Application espera los trabajos
    // antes de onShutdown.
    class ChunkStreamer {
    public:
        ChunkStreamer(VoxelWorld& world, StreamingSettings settings, JobSystem* jobs = nullptr);
        ~ChunkStreamer();
        ChunkStreamer(const ChunkStreamer&) = delete;
        ChunkStreamer& operator=(const ChunkStreamer&) = delete;

        // center: chunk de la cámara/jugador (solo cuentan X y Z: se cargan columnas enteras)
        StreamUpdate update(const ChunkCoord& center);

        StreamingStats stats() const;
        const StreamingSettings& settings() const { return m_settings; }
        void setSettings(StreamingSettings settings);

        // Limpia el registro de LODs y marca como sucios todos los chunks cargados para forzar su re-mallado
        void remeshAll();

        // Nivel de LOD actual asignado al chunk (-1 si no está mallado)
        int chunkLod(const ChunkCoord& coord) const;

        // Nivel de LOD objetivo para una coordenada según el centro y su LOD actual (con histéresis de 1
        // chunk)
        int targetLod(const ChunkCoord& coord, const ChunkCoord& center, int currentLod = -1) const;

        // ¿Todo lo que corresponde a center ya está cargado y mallado? (tests, pantallas de carga)
        bool idle() const { return m_generating.empty() && m_meshing.empty() && m_lastUpdateIdle; }

    private:
        static int columnDistance(const ChunkCoord& a, const ChunkCoord& b);
        bool neighboursReady(const ChunkCoord& coord) const;
        void collectGenerated(StreamUpdate& out, const ChunkCoord& center, int& budget);
        void collectMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget);
        void unloadFar(StreamUpdate& out, const ChunkCoord& center);
        void requestGeneration(const ChunkCoord& center, int& budget);
        void requestMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget);

        struct PendingMesh {
            std::future<ChunkMesh> future;
            int requestedLod = 0;
        };

        VoxelWorld& m_world;
        StreamingSettings m_settings;
        JobSystem* m_jobs;
        std::unordered_map<ChunkCoord, std::future<ChunkData>, ChunkCoordHash> m_generating;
        std::unordered_map<ChunkCoord, PendingMesh, ChunkCoordHash> m_meshing;
        std::unordered_map<ChunkCoord, int, ChunkCoordHash> m_chunkLod;
        std::size_t m_discarded = 0;
        bool m_lastUpdateIdle = false;
    };

} // namespace mge
