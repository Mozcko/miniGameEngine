#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../Engine/Graphics/3D/Mesh.h"
#include "../Engine/Graphics/Core/GLSLProgram.h"
#include "Block.h"

namespace MiniGameEngine {

    const int CHUNK_SIZE = 16;
    const int CHUNK_HEIGHT = 16;

    class Chunk {
    public:
        Chunk(int x, int z);
        ~Chunk();

        void generate();
        void buildMesh();
        void drawOpaque(GLSLProgram& shader);
        void drawTransparent(GLSLProgram& shader);
        
        Block getBlock(int x, int y, int z) const;
        glm::vec3 getPosition() const { return glm::vec3(_x * CHUNK_SIZE, 0, _z * CHUNK_SIZE); }
        glm::vec3 getCenter() const;
        float getRadius() const { return 14.0f; } // Radio aproximado para culling (sqrt(8^2+8^2+8^2) ~= 13.8)

    private:
        static int getTerrainHeight(int worldX, int worldZ);
        void placeTree(int x, int y, int z);

        int _x, _z;
        Block _blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
        Mesh* _meshOpaque;
        Mesh* _meshTransparent;
        
        // Helper para verificar límites internos
        Block getNeighborBlock(int x, int y, int z) const;
    };
}