#pragma once
#include <mge/render/3d/Mesh.h> // Heredamos de Mesh
#include <string>

namespace mge {

    // Sprite ES UN Mesh
    class Sprite : public Mesh {
    public:
        Sprite();
        ~Sprite() override; // Override opcional pero buena práctica

        // Inicialización simple (todo el texture atlas)
        void init(float x, float y, float width, float height);

        // Inicialización avanzada (Texture Atlas / SpriteSheet)
        void initFromAtlas(float x, float y, float width, float height,
                           float textureW, float textureH,
                           float tileW, float tileH,
                           int tileIndex);

        void setPosition(float x, float y);
        
        // Métodos específicos de Sprite
        void setFlipX(bool flip);
        void setColor(float r, float g, float b, float a); // Nuevo: Teñir sprite

        // draw() ya viene heredado de Mesh, no necesitamos re-declararlo 
        // a menos que queramos hacer algo especial.

    private:
        // Guardamos el estado para poder reconstruir el mesh si algo cambia (flip, posición)
        float m_x, m_y, m_width, m_height;
        float m_texW, m_texH, m_tileW, m_tileH;
        int m_tileIndex;
        bool m_flipX;
        glm::vec4 m_color; // Color actual
    };
}