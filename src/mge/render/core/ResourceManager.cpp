#include <mge/render/core/ResourceManager.h>
#include <mge/core/Log.h>

namespace mge {

    // Inicialización del mapa estático (obligatorio en C++)
    std::map<std::string, Texture> ResourceManager::s_textureCache;

    Texture ResourceManager::getTexture(const std::string& texturePath) {

        // 1. Buscar en el caché usando un iterador
        auto it = s_textureCache.find(texturePath);

        // 2. Si el iterador NO llegó al final, significa que ENCONTRÓ la textura
        if (it != s_textureCache.end()) {
            // Devolvemos la textura ya existente (it->second es el valor del mapa)
            return it->second;
        }

        // 3. Si no la encontró, la cargamos desde cero
        MGE_LOG_DEBUG("Resources", "Cargando textura desde disco: ", texturePath);

        Texture newTexture;
        // Asumimos que tu clase Texture tiene un método .load() que recibe el path
        // Si tu método load devuelve false al fallar, podrías agregar manejo de errores aquí.
        newTexture.load(texturePath);

        // 4. Guardamos la nueva textura en el mapa para el futuro
        s_textureCache[texturePath] = newTexture;

        // 5. Devolvemos la textura recién creada
        return newTexture;
    }

    void ResourceManager::clear() {
        // Esto borra el mapa, pero OJO:
        // Si tus texturas no borran la memoria de GPU en su destructor,
        // podrías tener fugas de memoria aquí.
        // Si tu clase Texture llama a glDeleteTextures en su destructor, esto funcionará bien.
        s_textureCache.clear();
        MGE_LOG_DEBUG("Resources", "Caché de texturas limpiado");
    }

}