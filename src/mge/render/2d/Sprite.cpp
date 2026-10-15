#include <mge/render/2d/Sprite.h>
#include <vector>
#include <cmath>
#include <algorithm> // swap

namespace MiniGameEngine {

    Sprite::Sprite() : _flipX(false), _color(1.0f, 1.0f, 1.0f, 1.0f) {
        // Inicializamos variables en 0
        _x = _y = _width = _height = 0;
        _texW = _texH = _tileW = _tileH = 0;
        _tileIndex = 0;
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
        _x = x; _y = y; _width = width; _height = height;
        _texW = textureW; _texH = textureH;
        _tileW = tileW; _tileH = tileH;
        _tileIndex = tileIndex;

        // 1. Cálculos de UV (Igual que antes)
        int numCols = (int)(textureW / tileW);
        int col = tileIndex % numCols;
        int row = tileIndex / numCols;

        float u_min = (col * tileW) / textureW;
        float u_max = ((col + 1) * tileW) / textureW;
        
        // Nota: En 2D a veces V se invierte, depende de tu shader.
        // Mantén la lógica que te funcionaba. Asumiré estándar OpenGL (1.0 - y).
        float v_max = 1.0f - (row * tileH) / textureH;
        float v_min = 1.0f - ((row + 1) * tileH) / textureH;

        if (_flipX) std::swap(u_min, u_max);

        // 2. Construir Vértices usando la estructura Vertex
        std::vector<Vertex> vertices(4);

        // BL (Bottom Left)
        vertices[0].setPosition(x, y, 0.0f);
        vertices[0].setUV(u_min, v_min);
        vertices[0].color = _color;

        // TL (Top Left)
        vertices[1].setPosition(x, y + height, 0.0f);
        vertices[1].setUV(u_min, v_max);
        vertices[1].color = _color;

        // TR (Top Right)
        vertices[2].setPosition(x + width, y + height, 0.0f);
        vertices[2].setUV(u_max, v_max);
        vertices[2].color = _color;

        // BR (Bottom Right)
        vertices[3].setPosition(x + width, y, 0.0f);
        vertices[3].setUV(u_max, v_min);
        vertices[3].color = _color;

        // 3. Índices (Cuadrado hecho de 2 triángulos)
        std::vector<unsigned int> indices = {
            0, 1, 2, // Triángulo 1
            2, 3, 0  // Triángulo 2
        };

        // 4. ¡MAGIA! Pasamos los datos a la clase padre Mesh
        Mesh::init(vertices, indices);
    }

    void Sprite::setPosition(float x, float y) {
        _x = x; 
        _y = y;
        // Reconstruimos el mesh con la nueva posición
        // (Nota: Para v1.0 esto está bien. Para v2.0 usaremos matrices de transformación)
        initFromAtlas(_x, _y, _width, _height, _texW, _texH, _tileW, _tileH, _tileIndex);
    }

    void Sprite::setFlipX(bool flip) {
        if (_flipX != flip) {
            _flipX = flip;
            initFromAtlas(_x, _y, _width, _height, _texW, _texH, _tileW, _tileH, _tileIndex);
        }
    }

    void Sprite::setColor(float r, float g, float b, float a) {
        _color = glm::vec4(r, g, b, a);
        initFromAtlas(_x, _y, _width, _height, _texW, _texH, _tileW, _tileH, _tileIndex);
    }
}