#include <mge/render/3d/Mesh.h>
#include <cstddef> // Para offsetof

namespace MiniGameEngine {

    Mesh::Mesh() : _vao(0), _vbo(0), _ebo(0), _indexCount(0) {}

    Mesh::~Mesh() {
        if (_vao) glDeleteVertexArrays(1, &_vao);
        if (_vbo) glDeleteBuffers(1, &_vbo);
        if (_ebo) glDeleteBuffers(1, &_ebo);
    }

    void Mesh::init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        _indexCount = indices.size();

        // Si ya existían, límpialos para re-usar el Mesh
        if (_vao) glDeleteVertexArrays(1, &_vao);
        if (_vbo) glDeleteBuffers(1, &_vbo);
        if (_ebo) glDeleteBuffers(1, &_ebo);

        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);

        glBindVertexArray(_vao);

        // VBO (Datos)
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // EBO (Índices)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // --- ATRIBUTOS (IMPORTANTE) ---
        
        // 1. Posición (location = 0) - vec3
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // 2. Color (location = 1) - vec4 (¡Nuevo!)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        // 3. UV (location = 2) - vec2
        // Nota: En tus shaders viejos UV era location 1. Asegúrate de actualizar tus shaders o cambiar este índice a 1.
        // Si usas el shader 3D, UV suele ser 1. Ajusta esto según tu shader.
        // Asumiremos location = 2 para Color y UV.
        // SI TU SHADER 2D NO TIENE COLOR, USA location = 1 PARA UV.
        // Estandarización: Pos=0, Color=1, UV=2. (Necesitarás actualizar shaders).
        
        // PARA COMPATIBILIDAD CON TUS SHADERS ACTUALES (Pos=0, UV=1), ignoraremos el color en el shader por ahora
        // O mejor: enviamos UV al 1 como siempre.
        glEnableVertexAttribArray(1); 
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        
        // (Opcional) Color en location 2 si decides usarlo en el shader
        // glEnableVertexAttribArray(2);
        // glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        glBindVertexArray(0);
    }

    void Mesh::draw() {
        if (_vao == 0) return;
        glBindVertexArray(_vao);
        glDrawElements(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}