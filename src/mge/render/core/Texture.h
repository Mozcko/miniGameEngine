#pragma once
#include <string>
#include <vector>
#include <GL/glew.h>

struct SDL_Surface;

namespace MiniGameEngine {

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

        void loadFromSurface(SDL_Surface* surface);

        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        GLuint getID() const { return _textureID; }

    private:
        GLuint _textureID;
        int _width;
        int _height;
        int _numChannels;

        std::vector<unsigned char> _originalPixels; 
    };

}