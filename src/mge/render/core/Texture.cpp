#include <mge/render/core/Texture.h>
#include <iostream>
#include <cstring> // Para std::memcpy
#include <SDL2/SDL.h>

// Definimos esto SOLO AQUÍ para que stb_image se compile
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace mge
{

    Texture::Texture() : m_textureId(0), m_width(0), m_height(0), m_numChannels(0)
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
        if (m_textureId != 0)
        {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
    }

    void Texture::load(const std::string &filePath)
    {
        // Si ya había una textura cargada en este objeto, límpiala primero
        if (m_textureId != 0)
        {
            // Ojo: solo si estás seguro de que nadie más la usa.
            // Por seguridad en load() solemos asumir que el objeto está vacío o reutilizándose.
            glDeleteTextures(1, &m_textureId);
        }

        // 1. Cargar los datos de la imagen (CPU)
        stbi_set_flip_vertically_on_load(true);

        unsigned char *data = stbi_load(filePath.c_str(), &m_width, &m_height, &m_numChannels, 0);

        if (!data)
        {
            std::cerr << "Error: No se pudo cargar la textura " << filePath << std::endl;
            return;
        }

        // Guardar copia de RAM (Opcional: Si necesitas ahorrar RAM en el futuro, borra esta línea)
        m_originalPixels.assign(data, data + m_width * m_height * m_numChannels);

        // 2. Generar la textura en OpenGL (GPU)
        glGenTextures(1, &m_textureId);
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // 3. Configuración Pixel Art (Minecraft Style)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Bordes seguros
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // 4. Formato
        GLenum format = GL_RGB;
        if (m_numChannels == 4)
        {
            format = GL_RGBA;
        }

        // 5. Enviar a VRAM
        glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, format, GL_UNSIGNED_BYTE, data);

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
        glBindTexture(GL_TEXTURE_2D, m_textureId);
    }

    void Texture::unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Mantenemos tu función de animación 2D
    void Texture::updateTile(int destTileIdx, int srcTileIdx, int tileW, int tileH)
    {
        if (m_originalPixels.empty())
            return;

        int cols = m_width / tileW;

        int srcCol = srcTileIdx % cols;
        int srcRow = srcTileIdx / cols;

        int destCol = destTileIdx % cols;
        int destRow = destTileIdx / cols;

        std::vector<unsigned char> tileData(tileW * tileH * m_numChannels);

        int srcStartY = m_height - (srcRow + 1) * tileH;
        int srcStartX = srcCol * tileW;

        for (int y = 0; y < tileH; y++)
        {
            int memoryY = srcStartY + y;
            int memoryX = srcStartX;

            int pixelIndex = (memoryY * m_width + memoryX) * m_numChannels;
            int destIndex = (y * tileW) * m_numChannels;

            std::memcpy(&tileData[destIndex], &m_originalPixels[pixelIndex], tileW * m_numChannels);
        }

        bind();

        int glDestY = m_height - (destRow + 1) * tileH;
        int glDestX = destCol * tileW;

        GLenum format = (m_numChannels == 4) ? GL_RGBA : GL_RGB;
        glTexSubImage2D(GL_TEXTURE_2D, 0, glDestX, glDestY, tileW, tileH, format, GL_UNSIGNED_BYTE, tileData.data());
    }

    void Texture::loadFromSurface(SDL_Surface *surface)
    {
        if (m_textureId != 0)
            glDeleteTextures(1, &m_textureId);

        m_width = surface->w;
        m_height = surface->h;
        m_numChannels = surface->format->BytesPerPixel;

        glGenTextures(1, &m_textureId);
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // Filtros
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Texto suave
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Detectar formato SDL (Suele ser RGBA o BGRA)
        GLenum mode = GL_RGB;
        if (m_numChannels == 4)
        {
            if (surface->format->Rmask == 0x000000ff)
                mode = GL_RGBA;
            else
                mode = GL_BGRA; // SDL a veces usa BGRA
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}