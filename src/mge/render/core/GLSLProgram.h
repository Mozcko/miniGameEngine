#pragma once

#include <string>
#include <GL/glew.h>

namespace mge {

    class GLSLProgram {
    public:
        GLSLProgram();
        ~GLSLProgram();

        // Compila los shaders desde archivos de texto (false si falla; el detalle va al log)
        bool compileShaders(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

        // Vincula los atributos (ej: "vertexPosition" -> índice 0)
        void addAttribute(const std::string& attributeName);

        // Termina de configurar el shader (false si falla el enlazado; el programa queda en 0)
        bool linkShaders();

        // Activa el shader para usarlo
        void use();

        // Desactiva el shader
        void unuse();

        // Busca la ubicación de una variable uniforme en el shader
        GLint getUniformLocation(const std::string& uniformName);

    private:
        // ID del programa en la GPU
        GLuint m_programId;
        GLuint m_vertexShaderId;
        GLuint m_fragmentShaderId;

        // Contador de atributos
        int m_numAttributes;

        // Función auxiliar para compilar un shader individual
        bool compileShader(const std::string& filePath, GLuint id);
    };
}