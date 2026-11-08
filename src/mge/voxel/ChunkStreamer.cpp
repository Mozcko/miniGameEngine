#include <mge/voxel/ChunkStreamer.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace mge {

    namespace {
        template <typename T> bool ready(const std::future<T>& future) {
            return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
    } // namespace

    ChunkStreamer::ChunkStreamer(VoxelWorld& world, StreamingSettings settings, JobSystem* jobs)
        : m_world(world), m_settings(std::move(settings)), m_jobs(jobs) {
        if (m_settings.lodEnabled()) {
            m_settings.loadRadius = std::max(m_settings.loadRadius, m_settings.maxMeshRadius() + 1);
        } else {
            m_settings.meshRadius = std::min(m_settings.meshRadius, m_settings.loadRadius - 1);
        }
    }

    ChunkStreamer::~ChunkStreamer() {
        // Los trabajos leen el generador y el registry: esperar a que terminen antes de soltarlos
        for (auto& [coord, future] : m_generating) {
            if (future.valid()) future.wait();
        }
        for (auto& [coord, pending] : m_meshing) {
            if (pending.future.valid()) pending.future.wait();
        }
    }

    void ChunkStreamer::setSettings(StreamingSettings settings) {
        m_settings = std::move(settings);
        if (m_settings.lodEnabled()) {
            m_settings.loadRadius = std::max(m_settings.loadRadius, m_settings.maxMeshRadius() + 1);
        } else {
            m_settings.meshRadius = std::min(m_settings.meshRadius, m_settings.loadRadius - 1);
        }
        remeshAll();
    }

    void ChunkStreamer::remeshAll() {
        m_chunkLod.clear();
        for (const ChunkCoord& c : m_world.loadedChunks()) {
            m_world.markDirty(c);
        }
    }

    int ChunkStreamer::columnDistance(const ChunkCoord& a, const ChunkCoord& b) {
        return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
    }

    int ChunkStreamer::chunkLod(const ChunkCoord& coord) const {
        auto it = m_chunkLod.find(coord);
        return it != m_chunkLod.end() ? it->second : -1;
    }

    int ChunkStreamer::targetLod(const ChunkCoord& coord, const ChunkCoord& center, int currentLod) const {
        if (!m_settings.lodEnabled()) {
            const int d = columnDistance(coord, center);
            return d <= m_settings.meshRadius ? 0 : -1;
        }

        const int d = columnDistance(coord, center);
        const int r0 = m_settings.lodRadius[0];
        const int r1 = m_settings.lodRadius[1];
        const int r2 = m_settings.lodRadius[2];
        const int maxR = m_settings.maxMeshRadius();

        if (d > maxR) return -1;

        // Histéresis de 1 chunk para evitar parpadeo en las fronteras de los anillos
        if (currentLod == 0) {
            if (d <= r0 + 1) return 0;
            if (r1 > 0 && d <= r1 + 1) return 1;
            if (r2 > 0 && d <= r2) return 2;
            return -1;
        }
        if (currentLod == 1) {
            if (d <= r0) return 0;
            if (r1 > 0 && d <= r1 + 1) return 1;
            if (r2 > 0 && d <= r2) return 2;
            return -1;
        }
        if (currentLod == 2) {
            if (d <= r0) return 0;
            if (r1 > 0 && d <= r1) return 1;
            if (r2 > 0 && d <= r2) return 2;
            return -1;
        }

        // Sin LOD previo (currentLod < 0)
        if (d <= r0) return 0;
        if (r1 > 0 && d <= r1) return 1;
        if (r2 > 0 && d <= r2) return 2;
        return -1;
    }

    bool ChunkStreamer::neighboursReady(const ChunkCoord& c) const {
        // Los 4 vecinos horizontales con datos; arriba y abajo también, salvo fuera del rango vertical
        const ChunkCoord horizontal[4] = {
            {c.x - 1, c.y, c.z}, {c.x + 1, c.y, c.z}, {c.x, c.y, c.z - 1}, {c.x, c.y, c.z + 1}};
        for (const ChunkCoord& n : horizontal) {
            if (!m_world.isLoaded(n)) return false;
        }
        if (c.y > m_settings.minChunkY && !m_world.isLoaded({c.x, c.y - 1, c.z})) return false;
        if (c.y < m_settings.maxChunkY && !m_world.isLoaded({c.x, c.y + 1, c.z})) return false;
        return true;
    }

    StreamUpdate ChunkStreamer::update(const ChunkCoord& center) {
        StreamUpdate out;
        int budget = std::max(1, m_settings.maxResultsPerUpdate);
        collectGenerated(out, center, budget);
        collectMeshes(out, center, budget);
        unloadFar(out, center);
        requestGeneration(center, budget);
        requestMeshes(out, center, budget);
        // Ocioso: nada en curso y nada nuevo que hacer este update (budget intacto tras pedir)
        m_lastUpdateIdle = budget == std::max(1, m_settings.maxResultsPerUpdate) && out.meshes.empty() &&
                           m_generating.empty() && m_meshing.empty();
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

    void ChunkStreamer::collectMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget) {
        for (auto it = m_meshing.begin(); it != m_meshing.end() && budget > 0;) {
            if (!ready(it->second.future)) {
                ++it;
                continue;
            }
            ChunkMesh mesh = it->second.future.get();
            const ChunkCoord coord = it->first;
            const int requestedLod = it->second.requestedLod;

            // Viejo si el chunk se descargó, se volvió a ensuciar, o cambió de anillo LOD mientras se mallaba
            const int prevLod = chunkLod(coord);
            const int curLod = targetLod(coord, center, prevLod);

            if (m_world.isLoaded(coord) && !m_world.isDirty(coord) && curLod == requestedLod && curLod >= 0) {
                out.meshes.push_back({coord, std::move(mesh), requestedLod});
                m_chunkLod[coord] = requestedLod;
                budget--;
            } else {
                m_discarded++;
                if (m_world.isLoaded(coord) && (curLod != requestedLod || curLod < 0)) {
                    m_world.markDirty(coord);
                }
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
            m_chunkLod.erase(c);
            out.unloaded.push_back(c);
        }

        // Presupuesto de memoria: los más lejanos primero, nunca dentro del radio de malla
        const int maxR = m_settings.maxMeshRadius();
        if (m_settings.maxLoadedChunks == 0 || m_world.chunkCount() <= m_settings.maxLoadedChunks) {
            return;
        }
        loaded = m_world.loadedChunks();
        std::sort(loaded.begin(), loaded.end(), [&](const ChunkCoord& a, const ChunkCoord& b) {
            return columnDistance(a, center) > columnDistance(b, center);
        });
        for (const ChunkCoord& c : loaded) {
            if (m_world.chunkCount() <= m_settings.maxLoadedChunks) break;
            if (columnDistance(c, center) <= maxR) break;
            m_world.unloadChunk(c);
            m_chunkLod.erase(c);
            out.unloaded.push_back(c);
        }
    }

    void ChunkStreamer::requestGeneration(const ChunkCoord& center, int& budget) {
        const TerrainGenerator* generator = m_world.generator();
        std::vector<ChunkCoord> missing;
        const int r = m_settings.loadRadius;
        for (int x = center.x - r; x <= center.x + r; x++) {
            for (int z = center.z - r; z <= center.z + r; z++) {
                for (int y = m_settings.minChunkY; y <= m_settings.maxChunkY; y++) {
                    const ChunkCoord c{x, y, z};
                    if (!m_world.isLoaded(c) && m_generating.count(c) == 0) missing.push_back(c);
                }
            }
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
                if (static_cast<int>(m_generating.size() + m_meshing.size()) >= m_settings.maxJobsInFlight) {
                    break;
                }
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
        const int maxR = m_settings.maxMeshRadius();

        // 1. Chunks sucios dentro del radio
        for (const ChunkCoord& c : m_world.dirtyChunks()) {
            if (columnDistance(c, center) > maxR || m_meshing.count(c) != 0) continue;
            if (neighboursReady(c)) candidates.push_back(c);
        }

        // 2. Chunks ya mallados cuyo anillo de LOD cambió
        for (const auto& [c, curLod] : m_chunkLod) {
            if (m_meshing.count(c) != 0 || m_world.isDirty(c)) continue;
            if (!m_world.isLoaded(c)) continue;
            const int nextLod = targetLod(c, center, curLod);
            if (nextLod >= 0 && nextLod != curLod && neighboursReady(c)) {
                candidates.push_back(c);
            }
        }

        std::sort(candidates.begin(), candidates.end(), [&](const ChunkCoord& a, const ChunkCoord& b) {
            return columnDistance(a, center) < columnDistance(b, center);
        });

        const BlockRegistry& registry = m_world.registry();
        for (const ChunkCoord& c : candidates) {
            const int prevLod = chunkLod(c);
            const int lod = targetLod(c, center, prevLod);
            if (lod < 0) continue;

            if (m_jobs) {
                if (static_cast<int>(m_generating.size() + m_meshing.size()) >= m_settings.maxJobsInFlight) {
                    break;
                }
                // El snapshot se toma acá (hilo principal); el trabajo solo lo lee
                auto snapshot = std::make_shared<ChunkSnapshot>(ChunkSnapshot::capture(m_world, c));
                m_meshing.emplace(
                    c,
                    PendingMesh{m_jobs->submit([snapshot, &registry, lod, options = m_settings.meshOptions] {
                                    return buildLodChunkMesh(*snapshot, registry, lod, options);
                                }),
                                lod});
                m_world.clearDirty(
                    c); // Si se edita mientras tanto vuelve a ensuciarse y el resultado se descarta
            } else {
                if (budget <= 0) break;
                const ChunkSnapshot snapshot = ChunkSnapshot::capture(m_world, c);
                out.meshes.push_back(
                    {c, buildLodChunkMesh(snapshot, registry, lod, m_settings.meshOptions), lod});
                m_chunkLod[c] = lod;
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

} // namespace mge
