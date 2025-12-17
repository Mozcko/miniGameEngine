#pragma once
#include <string>
#include <SDL2/SDL_ttf.h>
#include <glm/glm.hpp>
#include "../Core/Texture.h"
#include "Sprite.h"


namespace MiniGameEngine {

    class GLSLProgram;

    class GUILabel {
    public:
        GUILabel();
        ~GUILabel();

        // Inicializa con ruta de fuente y tamaño
        void init(const std::string& fontPath, int size);

        // Actualiza el texto (Solo regenera textura si el texto cambió)
        void setText(const std::string& text, const glm::vec4& color = glm::vec4(1.0f));

        // Dibuja en pantalla (Usa coordenadas de pantalla, ej: 10, 10 para esquina superior)
        // Requiere pasarle el shader de texturas (el 2D) y la cámara ortográfica
        void draw(GLSLProgram& shader, const glm::mat4& cameraMatrix);

        void setPosition(float x, float y) { _position = glm::vec2(x, y); }
        void setScale(float scale) { _scale = scale; }

    private:
        TTF_Font* _font;
        Texture _texture;
        Sprite _sprite; // Usamos un sprite para dibujarlo
        
        std::string _text;
        glm::vec2 _position;
        float _scale;
        glm::vec4 _color;
    };
}