#include "GUILabel.h"
#include <iostream>
#include "../Core/GLSLProgram.h"
#include <glm/gtc/matrix_transform.hpp>

namespace MiniGameEngine {

    GUILabel::GUILabel() : _font(nullptr), _position(0.0f), _scale(1.0f), _color(0.0f) {}

    GUILabel::~GUILabel() {
        if (_font) TTF_CloseFont(_font);
        // Texture se destruye sola (o con destroy() si lo agregaste)
    }

    void GUILabel::init(const std::string& fontPath, int size) {
        // Abrir fuente
        _font = TTF_OpenFont(fontPath.c_str(), size);
        if (!_font) {
            std::cerr << "[GUILabel] Error cargando fuente: " << TTF_GetError() << std::endl;
        }
    }

    void GUILabel::setText(const std::string& text, const glm::vec4& color) {
        if (!_font) return;
        // Si el texto es igual, no regeneramos (Optimización)
        if (_text == text && _color == color) return;

        _text = text;
        _color = color;

        // Convertir color glm a SDL_Color (0-255)
        SDL_Color sdlColor = { 
            (Uint8)(color.r * 255), 
            (Uint8)(color.g * 255), 
            (Uint8)(color.b * 255), 
            (Uint8)(color.a * 255) 
        };

        // Renderizar texto a superficie temporal
        // Use TTF_RenderText_Blended para bordes suaves (Alpha blending)
        SDL_Surface* surface = TTF_RenderText_Blended(_font, text.c_str(), sdlColor);
        
        if (surface) {
            // Convertir superficie a nuestra Textura OpenGL
            _texture.loadFromSurface(surface);
            
            // Ajustar el sprite al tamaño del texto
            _sprite.init(0, 0, surface->w, surface->h);
            
            SDL_FreeSurface(surface);
        }
    }

    void GUILabel::draw(GLSLProgram& shader, const glm::mat4& cameraMatrix) {
        if (_text.empty()) return;

        shader.use();

        // 1. Enviar Matriz de Proyección (Ortho)
        GLint pLocation = shader.getUniformLocation("P");
        glUniformMatrix4fv(pLocation, 1, GL_FALSE, &(cameraMatrix[0][0]));

        // 2. Crear Matriz de Modelo (Posición + Escala)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(_position.x, _position.y, 0.0f));
        model = glm::scale(model, glm::vec3(_scale, _scale, 1.0f));

        // (Suponiendo que tu shader de textura 2D no tiene uniform "model" explícito 
        // y usas Sprite::draw directamente, tendrías que modificar el shader o pasar la posición de otra forma.
        // PERO, como estamos en v1.0, vamos a asumir que usas el texture.vert básico que SÍ suele multiplicar P * vec4(pos, 1.0)).
        
        // --- TRUCO RÁPIDO PARA GUI EN SHADER SIMPLE ---
        // Si tu shader 2D (texture.vert) no tiene matriz Model, 
        // simplemente actualizamos la posición del sprite antes de dibujar.
        _sprite.setPosition(_position.x, _position.y);
        // (Nota: El escalado no funcionará bien sin matriz Model, pero para v1.0 basta).
        
        _texture.bind();
        _sprite.draw(); 
        _texture.unbind();
        
        shader.unuse();
    }
}