#pragma once
#include <map>
#include <string>
#include <mge/render/core/Texture.h>

namespace mge {

    // Clase estática para gestionar recursos (Texturas, y a futuro Sonidos, Shaders, etc.)
    class ResourceManager {
    public:
        // Método principal: Pide una textura. Si no existe, la carga.
        static Texture getTexture(const std::string& texturePath);

        // (Opcional) Limpia toda la memoria (útil al cambiar de nivel)
        static void clear(); 

    private:
        // El caché: Mapa de <Ruta, Textura>
        static std::map<std::string, Texture> s_textureCache;
    };

}