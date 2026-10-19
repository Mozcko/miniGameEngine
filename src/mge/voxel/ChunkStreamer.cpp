#include <mge/voxel/ChunkStreamer.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace mge {

    namespace {
        template <typename T> bool ready(const std::future<T>& future) {
            return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
    }

    ChunkStreamer::ChunkStreamer(VoxelWorld& world, StreamingSettings settings, JobSystem* jobs)
        : m_world(world), m_settings(std::move(settings)), m_jobs(jobs) {
        m_settings.meshRadius = std::min(m_settings.meshRadius, m_settings.loadRadius - 1);
    }

    ChunkStreamer::~ChunkStreamer() {
        // Los trabajos leen el generador y el registry: esperar a que terminen antes de soltarlos
        for (auto& [coord, future] : m_generating)
            if (future.valid()) future.wait();
        for (auto& [coord, future] : m_meshing)
            if (future.valid()) future.wait();
    }

    int ChunkStreamer::columnDistance(const ChunkCoord& a, const ChunkCoord& b) {
        return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
    }

    bool ChunkStreamer::neighboursReady(const ChunkCoord& c) const {
        // Los 4 vecinos horizontales con datos; arriba y abajo también, salvo fuera del rango vertical
        const ChunkCoord horizontal[4] = {
            {c.x - 1, c.y, c.z}, {c.x + 1, c.y, c.z}, {c.x, c.y, c.z - 1}, {c.x, c.y, c.z + 1}};
        for (const ChunkCoord& n : horizontal)
            if (!m_world.isLoaded(n)) return false;
        if (c.y > m_settings.minChunkY && !m_world.isLoaded({c.x, c.y - 1, c.z})) return false;
        if (c.y < m_settings.maxChunkY && !m_world.isLoaded({c.x, c.y + 1, c.z})) return false;
        return true;
    }

    StreamUpdate ChunkStreamer::update(const ChunkCoord& center) {
        StreamUpdate out;
        int budget = std::max(1, m_settings.maxResultsPerUpdate);
        collectGenerated(out, center, budget);
        collectMeshes(out, budget);
        unloadFar(out, center);
        requestGeneration(center, budget);
        requestMeshes(out, center, budget);
        // Ocioso: nada en curso y nada nuevo que hacer este update (budget intacto tras pedir)
        m_lastUpdateIdle = budget == std::max(1, m_settings.maxResultsPerUpdate) && out.meshes.empty();
        return out;
    }

    void ChunkStreamer::collectGenerated(StreamUpdate& /*out*/, const ChunkCoord& center, int& budget) {
        for (auto it = m_generating.begin(); it != m_generating.end() && budget > 0;) {
            if (!ready(it->second)) {
                ++it;
                continue;
            }
            ChunkData data = it->second.get();
            // Si mientras tanto el centro se alejó, el resultado ya no hace falta
            if (columnDistance(it->first, center) <= m_settings.loadRadius + m_settings.unloadMargin) {
                m_world.insertChunk(it->first, data);
                budget--;
            } else {
                m_discarded++;
            }
            it = m_generating.erase(it);
        }
    }

    void ChunkStreamer::collectMeshes(StreamUpdate& out, int& budget) {
        for (auto it = m_meshing.begin(); it != m_meshing.end() && budget > 0;) {
            if (!ready(it->second)) {
                ++it;
                continue;
            }
            ChunkMesh mesh = it->second.get();
            // Viejo si el chunk se descargó o se volvió a ensuciar (editado) después de pedir la malla
            if (m_world.isLoaded(it->first) && !m_world.isDirty(it->first)) {
                out.meshes.emplace_back(it->first, std::move(mesh));
                budget--;
            } else {
                m_discarded++;
            }
            it = m_meshing.erase(it);
        }
    }

    void ChunkStreamer::unloadFar(StreamUpdate& out, const ChunkCoord& center) {
        const int unloadRadius = m_settings.loadRadius + m_settings.unloadMargin;
        std::vector<ChunkCoord> loaded = m_world.loadedChunks();
        for (const ChunkCoord& c : loaded) {
            if (columnDistance(c, center) <= unloadRadius) continue;
            m_world.unloadChunk(c);
            out.unloaded.push_back(c);
        }
        // Presupuesto de memoria: los más lejanos primero, nunca dentro del radio de malla
        if (m_settings.maxLoadedChunks == 0 || m_world.chunkCount() <= m_settings.maxLoadedChunks) return;
        loaded = m_world.loadedChunks();
        std::sort(loaded.begin(), loaded.end(), [&](const ChunkCoord& a, const ChunkCoord& b) {
            return columnDistance(a, center) > columnDistance(b, center);
        });
        for (const ChunkCoord& c : loaded) {
            if (m_world.chunkCount() <= m_settings.maxLoadedChunks) break;
            if (columnDistance(c, center) <= m_settings.meshRadius) break;
            m_world.unloadChunk(c);
            out.unloaded.push_back(c);
        }
    }

    void ChunkStreamer::requestGeneration(const ChunkCoord& center, int& budget) {
        const TerrainGenerator* generator = m_world.generator();
        std::vector<ChunkCoord> missing;
        const int r = m_settings.loadRadius;
        for (int x = center.x - r; x <= center.x + r; x++)
            for (int z = center.z - r; z <= center.z + r; z++)
                for (int y = m_settings.minChunkY; y <= m_settings.maxChunkY; y++) {
                    const ChunkCoord c{x, y, z};
                    if (!m_world.isLoaded(c) && m_generating.count(c) == 0) missing.push_back(c);
                }
        std::sort(missing.begin(), missing.end(), [&](const ChunkCoord& a, const ChunkCoord& b) {
            const int da = columnDistance(a, center), db = columnDistance(b, center);
            if (da != db) return da < db;
            return std::abs(a.x - center.x) + std::abs(a.z - center.z) <
                   std::abs(b.x - center.x) + std::abs(b.z - center.z);
        });
        // Respetar el presupuesto de memoria: no generar más de lo que se puede tener cargado
        std::size_t room =
            m_settings.maxLoadedChunks == 0
                ? missing.size()
                : (m_settings.maxLoadedChunks > m_world.chunkCount() + m_generating.size()
                       ? m_settings.maxLoadedChunks - m_world.chunkCount() - m_generating.size()
                       : 0);
        for (const ChunkCoord& c : missing) {
            if (room == 0) break;
            if (m_jobs) {
                if (static_cast<int>(m_generating.size() + m_meshing.size()) >= m_settings.maxJobsInFlight)
                    break;
                m_generating.emplace(c, m_jobs->submit([generator, c] {
                    ChunkData data;
                    if (generator) generator->generate(c, data);
                    return data;
                }));
            } else {
                if (budget <= 0) break;
                m_world.loadChunk(c); // Sin hilos: generar acá, con el mismo límite por update
                budget--;
            }
            room--;
        }
    }

    void ChunkStreamer::requestMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget) {
        std::vector<ChunkCoord> candidates;
        for (const ChunkCoord& c : m_world.dirtyChunks()) {
            if (columnDistance(c, center) > m_settings.meshRadius || m_meshing.count(c) != 0) continue;
            if (neighboursReady(c)) candidates.push_back(c);
        }
        std::sort(candidates.begin(), candidates.end(), [&](const ChunkCoord& a, const ChunkCoord& b) {
            return columnDistance(a, center) < columnDistance(b, center);
        });
        const BlockRegistry& registry = m_world.registry();
        for (const ChunkCoord& c : candidates) {
            if (m_jobs) {
                if (static_cast<int>(m_generating.size() + m_meshing.size()) >= m_settings.maxJobsInFlight)
                    break;
                // El snapshot se toma acá (hilo principal); el trabajo solo lo lee
                auto snapshot = std::make_shared<ChunkSnapshot>(ChunkSnapshot::capture(m_world, c));
                m_meshing.emplace(c, m_jobs->submit([snapshot, &registry, options = m_settings.meshOptions] {
                    return buildChunkMesh(*snapshot, registry, options);
                }));
                m_world.clearDirty(
                    c); // Si se edita mientras tanto vuelve a ensuciarse y el resultado se descarta
            } else {
                if (budget <= 0) break;
                out.meshes.emplace_back(c, buildChunkMesh(m_world, c, m_settings.meshOptions));
                m_world.clearDirty(c);
                budget--;
            }
        }
    }

    StreamingStats ChunkStreamer::stats() const {
        StreamingStats s;
        s.loaded = m_world.chunkCount();
        s.pendingGenerate = m_generating.size();
        s.pendingMesh = m_meshing.size();
        s.pooled = m_world.pooledCount();
        s.discarded = m_discarded;
        return s;
    }
}
