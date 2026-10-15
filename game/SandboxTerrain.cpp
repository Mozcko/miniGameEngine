#include <game/SandboxTerrain.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace sandbox {

    using mge::BlockId;
    using mge::ChunkData;
    using mge::kChunkSize;
    using mge::NoiseType;

    namespace {
        float smoothstep(float edge0, float edge1, float x) {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        mge::FractalSettings fractal(int octaves, float frequency) {
            mge::FractalSettings settings;
            settings.octaves = octaves;
            settings.frequency = frequency;
            return settings;
        }
    }

    SandboxTerrain::SandboxTerrain(const SandboxBlocks& blocks, TerrainSettings settings)
        : m_blocks(blocks), m_settings(settings), m_height(settings.seed), m_mountains(settings.seed + 1),
          m_temperature(settings.seed + 2), m_moisture(settings.seed + 3), m_caveA(settings.seed + 4),
          m_caveB(settings.seed + 5) {}

    SandboxTerrain::Column SandboxTerrain::column(int worldX, int worldZ) const {
        const float x = static_cast<float>(worldX);
        const float z = static_cast<float>(worldZ);

        // Altura: colinas suaves (fbm) + montañas (crestas) donde la máscara lo permite
        const float hills = m_height.fbm(NoiseType::Simplex, x, z, fractal(5, 0.008f));
        const float mask =
            smoothstep(0.1f, 0.5f, m_mountains.fbm(NoiseType::Simplex, x, z, fractal(3, 0.0025f)));
        const float ridges = m_mountains.ridged(NoiseType::Simplex, x, z, fractal(4, 0.012f));
        const float h = 24.0f + hills * 10.0f + mask * ridges * 30.0f;

        Column col{};
        col.height = std::clamp(static_cast<int>(std::floor(h)), 1, kMaxSurfaceHeight);

        // Bioma: costa junto al agua; montaña si es alta y está en la máscara; si no, clima
        const float temperature = m_temperature.fbm(NoiseType::Simplex, x, z, fractal(3, 0.003f));
        const float moisture = m_moisture.fbm(NoiseType::Simplex, x, z, fractal(3, 0.003f));
        if (col.height <= kWaterLevel + 1)
            col.biome = Biome::Shore;
        else if (mask > 0.3f && col.height > kWaterLevel + 14)
            col.biome = Biome::Mountains;
        else if (temperature > 0.1f && moisture < 0.0f)
            col.biome = Biome::Desert;
        else
            col.biome = Biome::Plains;

        // Generador determinista por columna (stream 1 = terreno)
        mge::Pcg32 rng(mge::hash2i(worldX, worldZ, m_settings.seed), 1);
        const int roll = rng.range(0, 1000);
        switch (col.biome) {
        case Biome::Shore: col.surface = col.filler = m_blocks.sand; break;
        case Biome::Desert: col.surface = col.filler = m_blocks.sand; break;
        case Biome::Mountains:
            col.surface = roll < 150 ? m_blocks.gravel : m_blocks.stone;
            col.filler = m_blocks.stone;
            break;
        case Biome::Plains:
            col.surface = roll < 50 ? m_blocks.gravel : roll < 55 ? m_blocks.lava : m_blocks.grass;
            col.filler = m_blocks.dirt;
            break;
        }
        return col;
    }

    bool SandboxTerrain::isCave(int worldX, int worldY, int worldZ, int surfaceHeight) const {
        // El piso (y = 0) y las 3 capas de arriba quedan intactos: sin agujeros al vacío ni a la superficie
        if (!m_settings.caves || worldY < 1 || worldY > surfaceHeight - 4) return false;
        // Túneles: donde dos ruidos 3D están ambos cerca de 0 (intersección de dos "láminas")
        const float x = static_cast<float>(worldX) * 0.045f;
        const float y = static_cast<float>(worldY) * 0.07f;
        const float z = static_cast<float>(worldZ) * 0.045f;
        const float a = m_caveA.simplex(x, y, z);
        if (std::abs(a) > 0.09f) return false;
        return std::abs(m_caveB.simplex(x, y, z)) < 0.09f;
    }

    void SandboxTerrain::generate(const mge::ChunkCoord& coord, ChunkData& chunk) const {
        const glm::ivec3 origin = mge::chunkOrigin(coord);

        // Columnas del chunk y del margen de árboles, calculadas una sola vez (el ruido es lo caro)
        constexpr int kSpan = kChunkSize + 2 * kTreeRadius;
        std::array<Column, kSpan * kSpan> columns;
        for (int i = 0; i < kSpan; i++)
            for (int k = 0; k < kSpan; k++)
                columns[i * kSpan + k] = column(origin.x - kTreeRadius + i, origin.z - kTreeRadius + k);
        auto columnAt = [&](int wx, int wz) -> const Column& {
            return columns[(wx - origin.x + kTreeRadius) * kSpan + (wz - origin.z + kTreeRadius)];
        };

        // PASADA 1: terreno base (y del mundo: sirve para chunks a cualquier altura)
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                const int wx = origin.x + x;
                const int wz = origin.z + z;
                const Column& col = columnAt(wx, wz);
                for (int y = 0; y < kChunkSize; y++) {
                    const int wy = origin.y + y;
                    BlockId id = mge::kAir;
                    if (wy <= 0) {
                        id = m_blocks.stone; // Piso del mundo (y debajo): roca maciza
                    } else if (wy > col.height) {
                        if (wy <= kWaterLevel) id = m_blocks.water;
                    } else if (isCave(wx, wy, wz, col.height)) {
                        id = mge::kAir;
                    } else if (wy == col.height) {
                        id = col.surface;
                    } else if (wy > col.height - 4) {
                        id = col.filler;
                    } else {
                        id = m_blocks.stone;
                    }
                    chunk.set(x, y, z, id);
                }
            }
        }

        // PASADA 2: decoración. Se recorren también las columnas a kTreeRadius fuera del chunk: sus árboles
        // pueden meter hojas aquí. El orden es el del mundo (x, luego z), igual en todos los chunks, así que
        // donde dos árboles se tocan el resultado es el mismo lo genere quien lo genere.
        const int bottom = origin.y;
        const int top = origin.y + kChunkSize - 1;
        for (int wx = origin.x - kTreeRadius; wx < origin.x + kChunkSize + kTreeRadius; wx++) {
            for (int wz = origin.z - kTreeRadius; wz < origin.z + kChunkSize + kTreeRadius; wz++) {
                const Column& col = columnAt(wx, wz);
                // Lo más alto de un árbol es superficie + 7 (tronco de 5 + copa); si no toca este chunk,
                // saltar
                if (col.height + 7 < bottom || col.height + 1 > top) continue;
                if (col.surface != m_blocks.grass) continue;

                // Misma columna, otro stream (2 = decoración)
                mge::Pcg32 rng(mge::hash2i(wx, wz, m_settings.seed), 2);
                const int r = rng.range(0, 100);
                const glm::ivec3 base(wx, col.height + 1, wz); // Encima de la superficie (mundo)

                if (r < 2) { // 2 % árbol
                    placeTree(chunk, origin, base, rng);
                } else if (r < 6) { // 2 % rosa, 2 % margarita: solo en su columna
                    chunk.set(base - origin, r < 4 ? m_blocks.rose : m_blocks.flower);
                }
            }
        }
    }

    void SandboxTerrain::placeTree(ChunkData& chunk, const glm::ivec3& origin, const glm::ivec3& base,
                                   mge::Pcg32& rng) const {
        const int treeHeight = rng.range(4, 6); // 4 o 5

        // Copa: rombo de radio 2 sin las esquinas lejanas; solo ocupa aire. Se escribe la parte que cae
        // dentro de este chunk (ChunkData::set ignora lo de afuera).
        for (int y = base.y + treeHeight - 2; y <= base.y + treeHeight + 1; y++) {
            for (int dx = -kTreeRadius; dx <= kTreeRadius; dx++) {
                for (int dz = -kTreeRadius; dz <= kTreeRadius; dz++) {
                    if (std::abs(dx) + std::abs(dz) >= 4) continue;
                    const glm::ivec3 local = glm::ivec3(base.x + dx, y, base.z + dz) - origin;
                    if (ChunkData::inside(local.x, local.y, local.z) && chunk.get(local) == mge::kAir)
                        chunk.set(local, m_blocks.leaves);
                }
            }
        }

        // Tronco
        for (int i = 0; i < treeHeight; i++) chunk.set(base + glm::ivec3(0, i, 0) - origin, m_blocks.log);
    }
}
