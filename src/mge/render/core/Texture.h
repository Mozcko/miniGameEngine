#pragma once
#include <string>
#include <vector>
#include <GL/glew.h>

namespace mge {

    class Texture {
    public:
        Texture();
        ~Texture();

        void load(const std::string& filePath);

        // NUEVO: Método explícito para borrar la textura
        void destroy();

        void bind();
        void unbind();

        void updateTile(int destTileIdx, int srcTileIdx, int tileW, int tileH);

        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        GLuint getID() const { return m_textureId; }

    private:
        GLuint m_textureId;
        int m_width;
        int m_height;
        int m_numChannels;

        std::vector<unsigned char> m_originalPixels;
    };

}