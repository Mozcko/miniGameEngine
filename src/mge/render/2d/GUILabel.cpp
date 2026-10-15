#include <mge/render/2d/GUILabel.h>
#include <iostream>
#include <mge/render/core/GLSLProgram.h>
#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    GUILabel::GUILabel() : m_font(nullptr), m_position(0.0f), m_scale(1.0f), m_color(0.0f) {}

    GUILabel::~GUILabel() {
        if (m_font) TTF_CloseFont(m_font);
        // Texture se destruye sola (o con destroy() si lo agregaste)
    }

    void GUILabel::init(const std::string& fontPath, int size) {
        // Abrir fuente
        m_font = TTF_OpenFont(fontPath.c_str(), size);
        if (!m_font) {
            std::cerr << "[GUILabel] Error cargando fuente: " << TTF_GetError() << std::endl;
        }
    }

    void GUILabel::setText(const std::string& text, const glm::vec4& color) {
        if (!m_font) return;
        // Si el texto es igual, no regeneramos (Optimización)
        if (m_text == text && m_color == color) return;

        m_text = text;
        m_color = color;

        // Convertir color glm a SDL_Color (0-255)
        SDL_Color sdlColor = { 
            (Uint8)(color.r * 255), 
            (Uint8)(color.g * 255), 
            (Uint8)(color.b * 255), 
            (Uint8)(color.a * 255) 
        };

        // Renderizar texto a superficie temporal
        // Use TTF_RenderText_Blended para bordes suaves (Alpha blending)
        SDL_Surface* surface = TTF_RenderText_Blended(m_font, text.c_str(), sdlColor);
        
        if (surface) {
            // Convertir superficie a nuestra Textura OpenGL
            m_texture.loadFromSurface(surface);
            
            // Ajustar el sprite al tamaño del texto
            m_sprite.init(0, 0, surface->w, surface->h);
            
            SDL_FreeSurface(surface);
        }
    }

    void GUILabel::draw(GLSLProgram& shader, const glm::mat4& cameraMatrix) {
        if (m_text.empty()) return;

        shader.use();

        // 1. Enviar Matriz de Proyección (Ortho)
        GLint pLocation = shader.getUniformLocation("P");
        glUniformMatrix4fv(pLocation, 1, GL_FALSE, &(cameraMatrix[0][0]));

        // 2. Crear Matriz de Modelo (Posición + Escala)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(m_position.x, m_position.y, 0.0f));
        model = glm::scale(model, glm::vec3(m_scale, m_scale, 1.0f));

        // (Suponiendo que tu shader de textura 2D no tiene uniform "model" explícito 
        // y usas Sprite::draw directamente, tendrías que modificar el shader o pasar la posición de otra forma.
        // PERO, como estamos en v1.0, vamos a asumir que usas el texture.vert básico que SÍ suele multiplicar P * vec4(pos, 1.0)).
        
        // --- TRUCO RÁPIDO PARA GUI EN SHADER SIMPLE ---
        // Si tu shader 2D (texture.vert) no tiene matriz Model, 
        // simplemente actualizamos la posición del sprite antes de dibujar.
        m_sprite.setPosition(m_position.x, m_position.y);
        // (Nota: El escalado no funcionará bien sin matriz Model, pero para v1.0 basta).
        
        m_texture.bind();
        m_sprite.draw(); 
        m_texture.unbind();
        
        shader.unuse();
    }
}