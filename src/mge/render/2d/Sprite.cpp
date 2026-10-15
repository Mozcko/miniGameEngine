#include <mge/render/2d/Sprite.h>
#include <vector>
#include <cmath>
#include <algorithm> // swap

namespace mge {

    Sprite::Sprite() : m_flipX(false), m_color(1.0f, 1.0f, 1.0f, 1.0f) {
        // Inicializamos variables en 0
        m_x = m_y = m_width = m_height = 0;
        m_texW = m_texH = m_tileW = m_tileH = 0;
        m_tileIndex = 0;
    }

    Sprite::~Sprite() {
        // El destructor de Mesh se encarga de limpiar buffers
    }

    void Sprite::init(float x, float y, float width, float height) {
        // Init por defecto: textura completa (1x1 tiles)
        initFromAtlas(x, y, width, height, width, height, width, height, 0);
    }

    void Sprite::initFromAtlas(float x, float y, float width, float height,
                               float textureW, float textureH,
                               float tileW, float tileH,
                               int tileIndex) {
        m_x = x; m_y = y; m_width = width; m_height = height;
        m_texW = textureW; m_texH = textureH;
        m_tileW = tileW; m_tileH = tileH;
        m_tileIndex = tileIndex;

        // 1. Cálculos de UV (Igual que antes)
        int numCols = (int)(textureW / tileW);
        int col = tileIndex % numCols;
        int row = tileIndex / numCols;

        float uMin = (col * tileW) / textureW;
        float uMax = ((col + 1) * tileW) / textureW;
        
        // Nota: En 2D a veces V se invierte, depende de tu shader.
        // Mantén la lógica que te funcionaba. Asumiré estándar OpenGL (1.0 - y).
        float vMax = 1.0f - (row * tileH) / textureH;
        float vMin = 1.0f - ((row + 1) * tileH) / textureH;

        if (m_flipX) std::swap(uMin, uMax);

        // 2. Construir Vértices usando la estructura Vertex
        std::vector<Vertex> vertices(4);

        // BL (Bottom Left)
        vertices[0].setPosition(x, y, 0.0f);
        vertices[0].setUV(uMin, vMin);
        vertices[0].color = m_color;

        // TL (Top Left)
        vertices[1].setPosition(x, y + height, 0.0f);
        vertices[1].setUV(uMin, vMax);
        vertices[1].color = m_color;

        // TR (Top Right)
        vertices[2].setPosition(x + width, y + height, 0.0f);
        vertices[2].setUV(uMax, vMax);
        vertices[2].color = m_color;

        // BR (Bottom Right)
        vertices[3].setPosition(x + width, y, 0.0f);
        vertices[3].setUV(uMax, vMin);
        vertices[3].color = m_color;

        // 3. Índices (Cuadrado hecho de 2 triángulos)
        std::vector<unsigned int> indices = {
            0, 1, 2, // Triángulo 1
            2, 3, 0  // Triángulo 2
        };

        // 4. ¡MAGIA! Pasamos los datos a la clase padre Mesh
        Mesh::init(vertices, indices);
    }

    void Sprite::setPosition(float x, float y) {
        m_x = x; 
        m_y = y;
        // Reconstruimos el mesh con la nueva posición
        // (Nota: Para v1.0 esto está bien. Para v2.0 usaremos matrices de transformación)
        initFromAtlas(m_x, m_y, m_width, m_height, m_texW, m_texH, m_tileW, m_tileH, m_tileIndex);
    }

    void Sprite::setFlipX(bool flip) {
        if (m_flipX != flip) {
            m_flipX = flip;
            initFromAtlas(m_x, m_y, m_width, m_height, m_texW, m_texH, m_tileW, m_tileH, m_tileIndex);
        }
    }

    void Sprite::setColor(float r, float g, float b, float a) {
        m_color = glm::vec4(r, g, b, a);
        initFromAtlas(m_x, m_y, m_width, m_height, m_texW, m_texH, m_tileW, m_tileH, m_tileIndex);
    }
}