#include "Chunk.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib> // Para rand() y srand()

namespace MiniGameEngine
{

    Chunk::Chunk(int x, int z) : _x(x), _z(z), _meshOpaque(nullptr), _meshTransparent(nullptr)
    {
        generate();
        buildMesh();
    }

    Chunk::~Chunk()
    {
        if (_meshOpaque)
            delete _meshOpaque;
        if (_meshTransparent)
            delete _meshTransparent;
    }

    int Chunk::getTerrainHeight(int worldX, int worldZ)
    {
        // Generación de terreno simple (Olas)
        float heightVal = (sin(worldX * 0.2f) + cos(worldZ * 0.2f)) * 3.0f + 6.0f;
        int height = (int)heightVal;
        if (height < 1)
            height = 1;
        if (height >= CHUNK_HEIGHT)
            height = CHUNK_HEIGHT - 1;
        return height;
    }

    void Chunk::generate()
    {
        const int WATER_LEVEL = 5;

        // PASADA 1: Terreno Base
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                // Coordenadas globales para que el ruido coincida entre chunks
                int worldX = _x * CHUNK_SIZE + x;
                int worldZ = _z * CHUNK_SIZE + z;

                // Semilla determinista por columna
                srand(worldX * 8921 + worldZ * 33771);

                int height = getTerrainHeight(worldX, worldZ);

                // Determinar tipo de superficie
                BlockType surface = BlockType::Grass;
                if (height <= WATER_LEVEL + 1)
                {
                    surface = BlockType::Sand; // Playas
                }
                else if ((rand() % 100) < 5)
                {
                    surface = BlockType::Gravel; // Parches de grava
                }

                // Probabilidad de piscina de lava (rara)
                bool lavaPool = (rand() % 200 == 0) && height > WATER_LEVEL;

                for (int y = 0; y < CHUNK_HEIGHT; y++)
                {
                    if (y > height)
                    {
                        if (y <= WATER_LEVEL)
                        {
                            _blocks[x][y][z] = Block(BlockType::Water);
                        }
                        else
                        {
                            _blocks[x][y][z] = Block(BlockType::Air);
                        }
                    }
                    else if (y == height)
                    {
                        if (lavaPool)
                            _blocks[x][y][z] = Block(BlockType::Lava);
                        else
                            _blocks[x][y][z] = Block(surface);
                    }
                    else if (y > height - 3)
                    {
                        _blocks[x][y][z] = Block(BlockType::Dirt);
                    }
                    else
                    {
                        _blocks[x][y][z] = Block(BlockType::Stone);
                    }
                }
            }
        }

        // PASADA 2: Decoración (Árboles y Flores)
        // Lo hacemos en otro bucle para no sobrescribir árboles con el terreno de la siguiente iteración
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                int worldX = _x * CHUNK_SIZE + x;
                int worldZ = _z * CHUNK_SIZE + z;
                srand(worldX * 8921 + worldZ * 33771); // Misma semilla para consistencia

                int height = getTerrainHeight(worldX, worldZ);
                BlockType surface = _blocks[x][height][z].getType();

                // Solo decorar sobre Pasto y encima del nivel del agua
                if (surface == BlockType::Grass && height > WATER_LEVEL)
                {
                    int r = rand() % 100;
                    int y = height + 1;

                    if (y >= CHUNK_HEIGHT)
                        continue;

                    if (r < 2)
                    { // 2% Árbol
                        placeTree(x, y, z);
                    }
                    else if (r < 4)
                    { // 2% Rosa
                        _blocks[x][y][z] = Block(BlockType::Rose);
                    }
                    else if (r < 6)
                    { // 2% Margarita
                        _blocks[x][y][z] = Block(BlockType::Flower);
                    }
                }
            }
        }
    }

    void Chunk::placeTree(int x, int y, int z)
    {
        int treeHeight = 4 + (rand() % 2); // Altura 4 o 5

        // Hojas (Copa del árbol)
        for (int ly = y + treeHeight - 2; ly <= y + treeHeight + 1; ly++)
        {
            for (int lx = x - 2; lx <= x + 2; lx++)
            {
                for (int lz = z - 2; lz <= z + 2; lz++)
                {
                    // Verificar límites del Chunk (simplificación: no dibujamos árboles que se salen del chunk)
                    if (lx >= 0 && lx < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE && ly >= 0 && ly < CHUNK_HEIGHT)
                    {
                        // Forma simple: quitar esquinas para hacerlo más redondo
                        int dX = abs(lx - x);
                        int dZ = abs(lz - z);
                        if ((dX + dZ) < 4)
                        { // Evitar esquinas lejanas
                            if (!_blocks[lx][ly][lz].isActive())
                            { // No sobrescribir tronco u otros bloques
                                _blocks[lx][ly][lz] = Block(BlockType::Leaves);
                            }
                        }
                    }
                }
            }
        }

        // Tronco
        for (int i = 0; i < treeHeight; i++)
        {
            if (y + i < CHUNK_HEIGHT)
            {
                _blocks[x][y + i][z] = Block(BlockType::Log);
            }
        }
    }

    Block Chunk::getBlock(int x, int y, int z) const
    {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
            return Block(BlockType::Air);
        return _blocks[x][y][z];
    }

    Block Chunk::getNeighborBlock(int x, int y, int z) const
    {
        // Verificación rápida de altura global
        if (y < 0 || y >= CHUNK_HEIGHT)
            return Block(BlockType::Air);

        // Si estamos dentro del chunk, usamos los datos reales
        if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE)
        {
            return _blocks[x][y][z];
        }

        // Si es un vecino (fuera del array), usamos la matemática procedural para predecir si hay bloque
        int worldX = _x * CHUNK_SIZE + x;
        int worldZ = _z * CHUNK_SIZE + z;
        int height = getTerrainHeight(worldX, worldZ);

        if (y > height)
        {
            if (y <= 5)
                return Block(BlockType::Water); // Nivel del agua hardcoded a 5
            return Block(BlockType::Air);
        }
        // Asumimos piedra/tierra para el terreno vecino (opaco)
        return Block(BlockType::Stone);
    }

    void Chunk::buildMesh()
    {
        std::vector<Vertex> verticesOpaque;
        std::vector<unsigned int> indicesOpaque;
        int indexOffsetOpaque = 0;

        std::vector<Vertex> verticesTransparent;
        std::vector<unsigned int> indicesTransparent;
        int indexOffsetTransparent = 0;

        // Helper lambda corregido: Ahora incluye el COLOR (Blanco)
        auto makeVert = [](float x, float y, float z, float u, float v)
        {
            // Orden: Posición, Color, UV
            return Vertex{
                {x, y, z},                // Pos
                {1.0f, 1.0f, 1.0f, 1.0f}, // Color (R, G, B, A) <--- ¡ESTO FALTABA!
                {u, v}                    // UV
            };
        };

        std::vector<unsigned int> stdIndices = {0, 1, 2, 2, 3, 0};
        std::vector<unsigned int> invIndices = {1, 0, 3, 3, 2, 1};

        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
                for (int z = 0; z < CHUNK_SIZE; z++)
                {
                    Block currentBlock = _blocks[x][y][z];
                    if (!currentBlock.isActive())
                        continue;

                    BlockType type = currentBlock.getType();
                    glm::vec3 pos((float)x, (float)y, (float)z);

                    // Determinar a qué lista va este bloque
                    bool isTransparent = !currentBlock.isOpaque();
                    auto &vertices = isTransparent ? verticesTransparent : verticesOpaque;
                    auto &indices = isTransparent ? indicesTransparent : indicesOpaque;
                    int &indexOffset = isTransparent ? indexOffsetTransparent : indexOffsetOpaque;

                    // --- MODELO DE CRUZ (Flores, Vegetación) ---
                    if (type == BlockType::Rose || type == BlockType::Flower ||
                        type == BlockType::Sapling || type == BlockType::Cobweb)
                    {

                        UVRect uv = Block::getUVs(type, 0); // Usamos cara frontal para todo

                        auto addCrossVert = [&](float x, float y, float z, float u, float v)
                        {
                            Vertex vert = makeVert(x, y, z, u, v);
                            vert.position += pos;
                            vertices.push_back(vert);
                        };

                        // Diagonal 1
                        addCrossVert(-0.5f, -0.5f, -0.5f, uv.u_min, uv.v_min);
                        addCrossVert(0.5f, -0.5f, 0.5f, uv.u_max, uv.v_min);
                        addCrossVert(0.5f, 0.5f, 0.5f, uv.u_max, uv.v_max);
                        addCrossVert(-0.5f, 0.5f, -0.5f, uv.u_min, uv.v_max);
                        // Cara trasera (para que se vea por ambos lados)
                        addCrossVert(-0.5f, -0.5f, -0.5f, uv.u_min, uv.v_min);
                        addCrossVert(-0.5f, 0.5f, -0.5f, uv.u_min, uv.v_max);
                        addCrossVert(0.5f, 0.5f, 0.5f, uv.u_max, uv.v_max);
                        addCrossVert(0.5f, -0.5f, 0.5f, uv.u_max, uv.v_min);

                        // Diagonal 2
                        addCrossVert(-0.5f, -0.5f, 0.5f, uv.u_min, uv.v_min);
                        addCrossVert(0.5f, -0.5f, -0.5f, uv.u_max, uv.v_min);
                        addCrossVert(0.5f, 0.5f, -0.5f, uv.u_max, uv.v_max);
                        addCrossVert(-0.5f, 0.5f, 0.5f, uv.u_min, uv.v_max);
                        // Cara trasera
                        addCrossVert(-0.5f, -0.5f, 0.5f, uv.u_min, uv.v_min);
                        addCrossVert(-0.5f, 0.5f, 0.5f, uv.u_min, uv.v_max);
                        addCrossVert(0.5f, 0.5f, -0.5f, uv.u_max, uv.v_max);
                        addCrossVert(0.5f, -0.5f, -0.5f, uv.u_max, uv.v_min);

                        for (int q = 0; q < 4; q++)
                        {
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

                    auto addFace = [&](int faceIdx, const std::vector<unsigned int> &idxTemplate, glm::vec3 offset)
                    {
                        UVRect uv = Block::getUVs(type, faceIdx);
                        std::vector<Vertex> faceVerts;

                        // Definición de vértices locales (reutilizada de tu código anterior)
                        if (faceIdx == 0)
                            faceVerts = {makeVert(-0.5f, -0.5f, 0.5f, uv.u_min, uv.v_min), makeVert(0.5f, -0.5f, 0.5f, uv.u_max, uv.v_min), makeVert(0.5f, 0.5f, 0.5f, uv.u_max, uv.v_max), makeVert(-0.5f, 0.5f, 0.5f, uv.u_min, uv.v_max)};
                        else if (faceIdx == 1)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.u_max, uv.v_min), makeVert(0.5f, -0.5f, -0.5f, uv.u_min, uv.v_min), makeVert(0.5f, 0.5f, -0.5f, uv.u_min, uv.v_max), makeVert(-0.5f, 0.5f, -0.5f, uv.u_max, uv.v_max)};
                        else if (faceIdx == 2)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.u_min, uv.v_min), makeVert(-0.5f, -0.5f, 0.5f, uv.u_max, uv.v_min), makeVert(-0.5f, 0.5f, 0.5f, uv.u_max, uv.v_max), makeVert(-0.5f, 0.5f, -0.5f, uv.u_min, uv.v_max)};
                        else if (faceIdx == 3)
                            faceVerts = {makeVert(0.5f, -0.5f, 0.5f, uv.u_min, uv.v_min), makeVert(0.5f, -0.5f, -0.5f, uv.u_max, uv.v_min), makeVert(0.5f, 0.5f, -0.5f, uv.u_max, uv.v_max), makeVert(0.5f, 0.5f, 0.5f, uv.u_min, uv.v_max)};
                        else if (faceIdx == 4)
                            faceVerts = {makeVert(-0.5f, 0.5f, 0.5f, uv.u_min, uv.v_min), makeVert(0.5f, 0.5f, 0.5f, uv.u_max, uv.v_min), makeVert(0.5f, 0.5f, -0.5f, uv.u_max, uv.v_max), makeVert(-0.5f, 0.5f, -0.5f, uv.u_min, uv.v_max)};
                        else if (faceIdx == 5)
                            faceVerts = {makeVert(-0.5f, -0.5f, -0.5f, uv.u_min, uv.v_min), makeVert(0.5f, -0.5f, -0.5f, uv.u_max, uv.v_min), makeVert(0.5f, -0.5f, 0.5f, uv.u_max, uv.v_max), makeVert(-0.5f, -0.5f, 0.5f, uv.u_min, uv.v_max)};

                        for (const auto &v : faceVerts)
                        {
                            Vertex newV = v;
                            newV.position += offset;
                            vertices.push_back(newV);
                        }
                        for (unsigned int i : idxTemplate)
                            indices.push_back(i + indexOffset);
                        indexOffset += 4;
                    };

                    auto shouldDrawFace = [&](Block neighbor)
                    {
                        if (!neighbor.isActive())
                            return true; // Vecino es aire
                        if (!neighbor.isOpaque())
                        {
                            // Vecino es transparente (agua, vidrio, etc)
                            if (currentBlock.isOpaque())
                                return true; // Nosotros somos opacos, dibujar
                            if (currentBlock.getType() != neighbor.getType())
                                return true; // Distinto tipo transparente, dibujar
                        }
                        return false; // Vecino tapa la cara
                    };

                    if (shouldDrawFace(getNeighborBlock(x - 1, y, z)))
                        addFace(2, stdIndices, pos); // Left
                    if (shouldDrawFace(getNeighborBlock(x + 1, y, z)))
                        addFace(3, stdIndices, pos); // Right
                    if (shouldDrawFace(getNeighborBlock(x, y, z - 1)))
                        addFace(1, invIndices, pos); // Back
                    if (shouldDrawFace(getNeighborBlock(x, y, z + 1)))
                        addFace(0, stdIndices, pos); // Front
                    if (shouldDrawFace(getNeighborBlock(x, y + 1, z)))
                        addFace(4, stdIndices, pos); // Top
                    if (shouldDrawFace(getNeighborBlock(x, y - 1, z)))
                        addFace(5, stdIndices, pos); // Bottom
                }
            }
        }

        _meshOpaque = new Mesh();
        _meshOpaque->init(verticesOpaque, indicesOpaque);

        _meshTransparent = new Mesh();
        _meshTransparent->init(verticesTransparent, indicesTransparent);
    }

    void Chunk::drawOpaque(GLSLProgram &shader)
    {
        if (!_meshOpaque)
            return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), getPosition());
        glUniformMatrix4fv(shader.getUniformLocation("model"), 1, GL_FALSE, &model[0][0]);
        _meshOpaque->draw();
    }

    void Chunk::drawTransparent(GLSLProgram &shader)
    {
        if (!_meshTransparent)
            return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), getPosition());
        glUniformMatrix4fv(shader.getUniformLocation("model"), 1, GL_FALSE, &model[0][0]);
        _meshTransparent->draw();
    }

    glm::vec3 Chunk::getCenter() const
    {
        return getPosition() + glm::vec3(CHUNK_SIZE / 2.0f, CHUNK_HEIGHT / 2.0f, CHUNK_SIZE / 2.0f);
    }
}