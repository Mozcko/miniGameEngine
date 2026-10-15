#pragma once
#include <vector>
#include <GL/glew.h>
#include <mge/render/core/Vertex.h> // Incluimos la nueva estructura

namespace MiniGameEngine {

    class Mesh {
    public:
        Mesh();
        virtual ~Mesh(); // Virtual para que el destructor de Sprite se llame correctamente

        // Inicializa los buffers con la nueva estructura Vertex
        void init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        
        // Método virtual: Las clases hijas podrían querer dibujar diferente (ej: GL_LINES)
        virtual void draw();

    protected:
        GLuint _vao;
        GLuint _vbo;
        GLuint _ebo;
        int _indexCount;
    };
}