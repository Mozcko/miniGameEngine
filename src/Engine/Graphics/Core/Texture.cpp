#include "Texture.h"
#include <iostream>
#include <cstring> // Para std::memcpy
#include <SDL2/SDL.h>

// Definimos esto SOLO AQUÍ para que stb_image se compile
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace MiniGameEngine
{

    Texture::Texture() : _textureID(0), _width(0), _height(0), _numChannels(0)
    {
    }

    Texture::~Texture()
    {
        // --- CAMBIO IMPORTANTE PARA RESOURCE MANAGER ---
        // NO borramos la textura aquí automáticamente.
        // Como el ResourceManager y otras entidades comparten COPIAS de este objeto,
        // si uno se destruye, no queremos que borre la textura de la GPU para los demás.

        // Si necesitas borrarla explícitamente, llama a destroy().
    }

    void Texture::destroy()
    {
        if (_textureID != 0)
        {
            glDeleteTextures(1, &_textureID);
            _textureID = 0;
        }
    }

    void Texture::load(const std::string &filePath)
    {
        // Si ya había una textura cargada en este objeto, límpiala primero
        if (_textureID != 0)
        {
            // Ojo: solo si estás seguro de que nadie más la usa.
            // Por seguridad en load() solemos asumir que el objeto está vacío o reutilizándose.
            glDeleteTextures(1, &_textureID);
        }

        // 1. Cargar los datos de la imagen (CPU)
        stbi_set_flip_vertically_on_load(true);

        unsigned char *data = stbi_load(filePath.c_str(), &_width, &_height, &_numChannels, 0);

        if (!data)
        {
            std::cerr << "Error: No se pudo cargar la textura " << filePath << std::endl;
            return;
        }

        // Guardar copia de RAM (Opcional: Si necesitas ahorrar RAM en el futuro, borra esta línea)
        _originalPixels.assign(data, data + _width * _height * _numChannels);

        // 2. Generar la textura en OpenGL (GPU)
        glGenTextures(1, &_textureID);
        glBindTexture(GL_TEXTURE_2D, _textureID);

        // 3. Configuración Pixel Art (Minecraft Style)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Bordes seguros
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // 4. Formato
        GLenum format = GL_RGB;
        if (_numChannels == 4)
        {
            format = GL_RGBA;
        }

        // 5. Enviar a VRAM
        glTexImage2D(GL_TEXTURE_2D, 0, format, _width, _height, 0, format, GL_UNSIGNED_BYTE, data);

        // Mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);

        // 6. Liberar RAM de stbi (ya tenemos copia en _originalPixels o en GPU)
        stbi_image_free(data);

        // Desvincular
        glBindTexture(GL_TEXTURE_2D, 0);

        // Mensaje de debug útil
        // std::cout << "Textura cargada: " << filePath << " [" << _width << "x" << _height << "]" << std::endl;
    }

    void Texture::bind()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _textureID);
    }

    void Texture::unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Mantenemos tu función de animación 2D
    void Texture::updateTile(int destTileIdx, int srcTileIdx, int tileW, int tileH)
    {
        if (_originalPixels.empty())
            return;

        int cols = _width / tileW;

        int srcCol = srcTileIdx % cols;
        int srcRow = srcTileIdx / cols;

        int destCol = destTileIdx % cols;
        int destRow = destTileIdx / cols;

        std::vector<unsigned char> tileData(tileW * tileH * _numChannels);

        int srcStartY = _height - (srcRow + 1) * tileH;
        int srcStartX = srcCol * tileW;

        for (int y = 0; y < tileH; y++)
        {
            int memoryY = srcStartY + y;
            int memoryX = srcStartX;

            int pixelIndex = (memoryY * _width + memoryX) * _numChannels;
            int destIndex = (y * tileW) * _numChannels;

            std::memcpy(&tileData[destIndex], &_originalPixels[pixelIndex], tileW * _numChannels);
        }

        bind();

        int glDestY = _height - (destRow + 1) * tileH;
        int glDestX = destCol * tileW;

        GLenum format = (_numChannels == 4) ? GL_RGBA : GL_RGB;
        glTexSubImage2D(GL_TEXTURE_2D, 0, glDestX, glDestY, tileW, tileH, format, GL_UNSIGNED_BYTE, tileData.data());
    }

    void Texture::loadFromSurface(SDL_Surface *surface)
    {
        if (_textureID != 0)
            glDeleteTextures(1, &_textureID);

        _width = surface->w;
        _height = surface->h;
        _numChannels = surface->format->BytesPerPixel;

        glGenTextures(1, &_textureID);
        glBindTexture(GL_TEXTURE_2D, _textureID);

        // Filtros
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Texto suave
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Detectar formato SDL (Suele ser RGBA o BGRA)
        GLenum mode = GL_RGB;
        if (_numChannels == 4)
        {
            if (surface->format->Rmask == 0x000000ff)
                mode = GL_RGBA;
            else
                mode = GL_BGRA; // SDL a veces usa BGRA
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}