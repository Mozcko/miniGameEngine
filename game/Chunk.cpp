#include <game/Chunk.h>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    Chunk::Chunk(int x, int z, std::uint32_t worldSeed)
        : m_x(x), m_z(z), m_seed(worldSeed), m_meshOpaque(nullptr), m_meshTransparent(nullptr) {
        // Solo datos (CPU). La malla (GPU) se construye aparte con buildMesh(),
        // así la generación se puede probar sin contexto OpenGL.
        generate();
    }

    Chunk::~Chunk() {
        if (m_meshOpaque) delete m_meshOpaque;
        if (m_meshTransparent) delete m_meshTransparent;
    }

    int Chunk::getTerrainHeight(int worldX, int worldZ) {
        // Generación de terreno simple (Olas)
        float heightVal = (sin(worldX * 0.2f) + cos(worldZ * 0.2f)) * 3.0f + 6.0f;
        int height = (int)heightVal;
        if (height < 1) height = 1;
        if (height >= kChunkHeight) height = kChunkHeight - 1;
        return height;
    }

    void Chunk::generate() {
        const int waterLevel = 5;

        // PASADA 1: Terreno Base
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                // Coordenadas globales para que el ruido coincida entre chunks
                int worldX = m_x * kChunkSize + x;
                int worldZ = m_z * kChunkSize + z;

                // Generador determinista por columna (stream 1 = terreno). Sin estado global.
                Pcg32 rng(hash2i(worldX, worldZ, m_seed), 1);

                int height = getTerrainHeight(worldX, worldZ);

                // Determinar tipo de superficie
                BlockType surface = BlockType::Grass;
                if (height <= waterLevel + 1) {
                    surface = BlockType::Sand; // Playas
                } else if (rng.range(0, 100) < 5) {
                    surface = BlockType::Gravel; // Parches de grava
                }

                // Probabilidad de piscina de lava (rara)
                bool lavaPool = (rng.range(0, 200) == 0) && height > waterLevel;

                for (int y = 0; y < kChunkHeight; y++) {
                    if (y > height) {
                        if (y <= waterLevel) {
                            m_blocks[x][y][z] = Block(BlockType::Water);
                        } else {
                            m_blocks[x][y][z] = Block(BlockType::Air);
                        }
                    } else if (y == height) {
                        if (lavaPool)
                            m_blocks[x][y][z] = Block(BlockType::Lava);
                        else
                            m_blocks[x][y][z] = Block(surface);
                    } else if (y > height - 3) {
                        m_blocks[x][y][z] = Block(BlockType::Dirt);
                    } else {
                        m_blocks[x][y][z] = Block(BlockType::Stone);
                    }
                }
            }
        }

        // PASADA 2: Decoración (Árboles y Flores)
        // Lo hacemos en otro bucle para no sobrescribir árboles con el terreno de la siguiente iteración
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                int worldX = m_x * kChunkSize + x;
                int worldZ = m_z * kChunkSize + z;
                // Misma columna, otro stream (2 = decoración): independiente de las tiradas del terreno
                Pcg32 rng(hash2i(worldX, worldZ, m_seed), 2);

                int height = getTerrainHeight(worldX, worldZ);
                BlockType surface = m_blocks[x][height][z].getType();

                // Solo decorar sobre Pasto y encima del nivel del agua
                if (surface == BlockType::Grass && height > waterLevel) {
                    int r = rng.range(0, 100);
                    int y = height + 1;

                    if (y >= kChunkHeight) continue;

                    if (r < 2) { // 2% Árbol
                        placeTree(x, y, z, rng);
                    } else if (r < 4) { // 2% Rosa
                        m_blocks[x][y][z] = Block(BlockType::Rose);
                    } else if (r < 6) { // 2% Margarita
                        m_blocks[x][y][z] = Block(BlockType::Flower);
                    }
                }
            }
        }
    }

    void Chunk::placeTree(int x, int y, int z, Pcg32& rng) {
        int treeHeight = rng.range(4, 6); // Altura 4 o 5

        // Hojas (Copa del árbol)
        for (int ly = y + treeHeight - 2; ly <= y + treeHeight + 1; ly++) {
            for (int lx = x - 2; lx <= x + 2; lx++) {
                for (int lz = z - 2; lz <= z + 2; lz++) {
                    // Verificar límites del Chunk (simplificación: no dibujamos árboles que se salen del
                    // chunk)
                    if (lx >= 0 && lx < kChunkSize && lz >= 0 && lz < kChunkSize && ly >= 0 &&
                        ly < kChunkHeight) {
                        // Forma simple: quitar esquinas para hacerlo más redondo
                        int dX = abs(lx - x);
                        int dZ = abs(lz - z);
                        if ((dX + dZ) < 4) {                        // Evitar esquinas lejanas
                            if (!m_blocks[lx][ly][lz].isActive()) { // No sobrescribir tronco u otros bloques
                                m_blocks[lx][ly][lz] = Block(BlockType::Leaves);
                            }
                        }
                    }
                }
            }
        }

        // Tronco
        for (int i = 0; i < treeHeight; i++) {
            if (y + i < kChunkHeight) {
                m_blocks[x][y + i][z] = Block(BlockType::Log);
            }
        }
    }

    Block Chunk::getBlock(int x, int y, int z) const {
        if (x < 0 || x >= kChunkSize || y < 0 || y >= kChunkHeight || z < 0 || z >= kChunkSize)
            return Block(BlockType::Air);
        return m_blocks[x][y][z];
    }

    Block Chunk::getNeighborBlock(int x, int y, int z) const {
        // Verificación rápida de altura global
        if (y < 0 || y >= kChunkHeight) return Block(BlockType::Air);

        // Si estamos dentro del chunk, usamos los datos reales
        if (x >= 0 && x < kChunkSize && z >= 0 && z < kChunkSize) {
            return m_blocks[x][y][z];
        }

        // Si es un vecino (fuera del array), usamos la matemática procedural para predecir si hay bloque
        int worldX = m_x * kChunkSize + x;
        int worldZ = m_z * kChunkSize + z;
        int height = getTerrainHeight(worldX, worldZ);

        if (y > height) {
            if (y <= 5) return Block(BlockType::Water); // Nivel del agua hardcoded a 5
            return Block(BlockType::Air);
        }
        // Asumimos piedra/tierra para el terreno vecino (opaco)
        return Block(BlockType::Stone);
    }

    void Chunk::buildMesh() {
        std::vector<Vertex> verticesOpaque;
        std::vector<unsigned int> indicesOpaque;
        int indexOffsetOpaque = 0;

        std::vector<Vertex> verticesTransparent;
        std::vector<unsigned int> indicesTransparent;
        int indexOffsetTransparent = 0;

        // Helper lambda corregido: Ahora incluye el COLOR (Blanco)
        auto makeVert = [](float x, float y, float z, float u, float v) {
            // Orden: Posición, Color, UV
            return Vertex{
                {x, y, z},                // Pos
                {1.0f, 1.0f, 1.0f, 1.0f}, // Color (R, G, B, A) <--- ¡ESTO FALTABA!
                {u, v}                    // UV
            };
        };

        std::vector<unsigned int> stdIndices = {0, 1, 2, 2, 3, 0};
        std::vector<unsigned int> invIndices = {1, 0, 3, 3, 2, 1};

        for (int x = 0; x < kChunkSize; x++) {
            for (int y = 0; y < kChunkHeight; y++) {
                for (int z = 0; z < kChunkSize; z++) {
                    Block currentBlock = m_blocks[x][y][z];
                    if (!currentBlock.isActive()) continue;

                    BlockType type = currentBlock.getType();
                    glm::vec3 pos((float)x, (float)y, (float)z);

                    // Determinar a qué lista va este bloque
                    bool isTransparent = !currentBlock.isOpaque();
                    auto& vertices = isTransparent ? verticesTransparent : verticesOpaque;
                    auto& indices = isTransparent ? indicesTransparent : indicesOpaque;
                    int& indexOffset = isTransparent ? indexOffsetTransparent : indexOffsetOpaque;

                    // --- MODELO DE CRUZ (Flores, Vegetación) ---
                    if (type == BlockType::Rose || type == BlockType::Flower || type == BlockType::Sapling ||
                        type == BlockType::Cobweb) {

                        UVRect uv = Block::getUVs(type, 0); // Usamos cara frontal para todo

                        auto addCrossVert = [&](float vx, float vy, float vz, float u, float v) {
                            Vertex vert = makeVert(vx, vy, vz, u, v);
                            vert.position += pos;
                            vertices.push_back(vert);
                        };

                        // Diagonal 1
                        addCrossVert(-0.5f, -0.5f, -0.5f, uv.uMin, uv.vMin);
                        addCrossVert(0.5f, -0.5f, 0.5f, uv.uMax, uv.vMin);
                        addCrossVert(0.5f, 0.5f, 0.5f, uv.uMax, uv.vMax);
                        addCrossVert(-0.5f, 0.5f, -0.5f, uv.uMin, uv.vMax);
                        // Cara trasera (para que se vea por ambos lados)
                        addCrossVert(-0.5f, -0.5f, -0.5f, uv.uMin, uv.vMin);
                        addCrossVert(-0.5f, 0.5f, -0.5f, uv.uMin, uv.vMax);
                        addCrossVert(0.5f, 0.5f, 0.5f, uv.uMax, uv.vMax);
                        addCrossVert(0.5f, -0.5f, 0.5f, uv.uMax, uv.vMin);

                        // Diagonal 2
                        addCrossVert(-0.5f, -0.5f, 0.5f, uv.uMin, uv.vMin);
                        addCrossVert(0.5f, -0.5f, -0.5f, uv.uMax, uv.vMin);
                        addCrossVert(0.5f, 0.5f, -0.5f, uv.uMax, uv.vMax);
                        addCrossVert(-0.5f, 0.5f, 0.5f, uv.uMin, uv.vMax);
                        // Cara trasera
                        addCrossVert(-0.5f, -0.5f, 0.5f, uv.uMin, uv.vMin);
                        addCrossVert(-0.5f, 0.5f, 0.5f, uv.uMin, uv.vMax);
                        addCrossVert(0.5f, 0.5f, -0.5f, uv.uMax, uv.vMax);
                        addCrossVert(0.5f, -0.5f, -0.5f, uv.uMax, uv.vMin);

                        for (int q = 0; q < 4; q++) {
                            int b = q * 4;
                            indices.push_back(indexOffset + b);
                            indices.push_back(indexOffset + b + 1);
                            indices.push_back(indexOffset + b + 2);
                            indices.push_back(indexOffset + b + 2);
                            indices.push_back(indexOffset + b + 3);
                            indices.push_back(indexOffset + b);
                        }
                        indexOffset += 16;
                        continue;
                    }

                    auto addFace = [&](int faceIdx, const std::vector<unsigned int>& idxTemplate,
                                       glm::vec3 offset) {
                        UVRect uv = Block::getUVs(type, faceIdx);
                        std::vector<Vertex> faceVerts;

                        // Definición de vértices locales (reutilizada de tu código anterior)
                        if (faceIdx == 0)
                            faceVerts = {makeVert(-0.5f, -0.5f, 0.5f, uv.uMin, uv.vMin),
                                         makeVert(0.5f, -0.5f, 0.5f, uv.uMax, uv.vMin),
                                         makeVert(0.5f, 0.5f, 0.5f, uv.uMax, uv.vMax),
                                         makeVert(-0.5f, 0.5f, 0.5f, uv.uMin, uv.vMax)};
                        else if (faceIdx == 1)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.uMax, uv.vMin),
                                         makeVert(0.5f, -0.5f, -0.5f, uv.uMin, uv.vMin),
                                         makeVert(0.5f, 0.5f, -0.5f, uv.uMin, uv.vMax),
                                         makeVert(-0.5f, 0.5f, -0.5f, uv.uMax, uv.vMax)};
                        else if (faceIdx == 2)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.uMin, uv.vMin),
                                         makeVert(-0.5f, -0.5f, 0.5f, uv.uMax, uv.vMin),
                                         makeVert(-0.5f, 0.5f, 0.5f, uv.uMax, uv.vMax),
                                         makeVert(-0.5f, 0.5f, -0.5f, uv.uMin, uv.vMax)};
                        else if (faceIdx == 3)
                            faceVerts = {makeVert(0.5f, -0.5f, 0.5f, uv.uMin, uv.vMin),
                                         makeVert(0.5f, -0.5f, -0.5f, uv.uMax, uv.vMin),
                                         makeVert(0.5f, 0.5f, -0.5f, uv.uMax, uv.vMax),
                                         makeVert(0.5f, 0.5f, 0.5f, uv.uMin, uv.vMax)};
                        else if (faceIdx == 4)
                            faceVerts = {makeVert(-0.5f, 0.5f, 0.5f, uv.uMin, uv.vMin),
                                         makeVert(0.5f, 0.5f, 0.5f, uv.uMax, uv.vMin),
                                         makeVert(0.5f, 0.5f, -0.5f, uv.uMax, uv.vMax),
                                         makeVert(-0.5f, 0.5f, -0.5f, uv.uMin, uv.vMax)};
                        else if (faceIdx == 5)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.uMin, uv.vMin),
                                         makeVert(0.5f, -0.5f, -0.5f, uv.uMax, uv.vMin),
                                         makeVert(0.5f, -0.5f, 0.5f, uv.uMax, uv.vMax),
                                         makeVert(-0.5f, -0.5f, 0.5f, uv.uMin, uv.vMax)};

                        for (const auto& v : faceVerts) {
                            Vertex newV = v;
                            newV.position += offset;
                            vertices.push_back(newV);
                        }
                        for (unsigned int i : idxTemplate) indices.push_back(i + indexOffset);
                        indexOffset += 4;
                    };

                    auto shouldDrawFace = [&](Block neighbor) {
                        if (!neighbor.isActive()) return true; // Vecino es aire
                        if (!neighbor.isOpaque()) {
                            // Vecino es transparente (agua, vidrio, etc)
                            if (currentBlock.isOpaque()) return true; // Nosotros somos opacos, dibujar
                            if (currentBlock.getType() != neighbor.getType())
                                return true; // Distinto tipo transparente, dibujar
                        }
                        return false; // Vecino tapa la cara
                    };

                    if (shouldDrawFace(getNeighborBlock(x - 1, y, z))) addFace(2, stdIndices, pos); // Left
                    if (shouldDrawFace(getNeighborBlock(x + 1, y, z))) addFace(3, stdIndices, pos); // Right
                    if (shouldDrawFace(getNeighborBlock(x, y, z - 1))) addFace(1, invIndices, pos); // Back
                    if (shouldDrawFace(getNeighborBlock(x, y, z + 1))) addFace(0, stdIndices, pos); // Front
                    if (shouldDrawFace(getNeighborBlock(x, y + 1, z))) addFace(4, stdIndices, pos); // Top
                    if (shouldDrawFace(getNeighborBlock(x, y - 1, z))) addFace(5, stdIndices, pos); // Bottom
                }
            }
        }

        m_meshOpaque = new Mesh();
        m_meshOpaque->init(verticesOpaque, indicesOpaque);

        m_meshTransparent = new Mesh();
        m_meshTransparent->init(verticesTransparent, indicesTransparent);
    }

    void Chunk::drawOpaque(GLSLProgram& shader) {
        if (!m_meshOpaque) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), getPosition());
        glUniformMatrix4fv(shader.getUniformLocation("model"), 1, GL_FALSE, &model[0][0]);
        m_meshOpaque->draw();
    }

    void Chunk::drawTransparent(GLSLProgram& shader) {
        if (!m_meshTransparent) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), getPosition());
        glUniformMatrix4fv(shader.getUniformLocation("model"), 1, GL_FALSE, &model[0][0]);
        m_meshTransparent->draw();
    }

    glm::vec3 Chunk::getCenter() const {
        return getPosition() + glm::vec3(kChunkSize / 2.0f, kChunkHeight / 2.0f, kChunkSize / 2.0f);
    }
}