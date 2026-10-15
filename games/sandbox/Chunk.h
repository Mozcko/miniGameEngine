#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <mge/render/3d/Mesh.h>
#include <mge/render/core/GLSLProgram.h>
#include <sandbox/Block.h>

namespace mge {

    const int kChunkSize = 16;
    const int kChunkHeight = 16;

    class Chunk {
    public:
        Chunk(int x, int z); // Genera los bloques; llamar buildMesh() antes de dibujar
        ~Chunk();

        void generate();
        void buildMesh();
        void drawOpaque(GLSLProgram& shader);
        void drawTransparent(GLSLProgram& shader);

        Block getBlock(int x, int y, int z) const;
        glm::vec3 getPosition() const { return glm::vec3(m_x * kChunkSize, 0, m_z * kChunkSize); }
        glm::vec3 getCenter() const;
        float getRadius() const { return 14.0f; } // Radio aproximado para culling (sqrt(8^2+8^2+8^2) ~= 13.8)

    private:
        static int getTerrainHeight(int worldX, int worldZ);
        void placeTree(int x, int y, int z);

        int m_x, m_z;
        Block m_blocks[kChunkSize][kChunkHeight][kChunkSize];
        Mesh* m_meshOpaque;
        Mesh* m_meshTransparent;

        // Helper para verificar límites internos
        Block getNeighborBlock(int x, int y, int z) const;
    };
}