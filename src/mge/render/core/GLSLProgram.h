#pragma once

#include <string>
#include <GL/glew.h>

namespace MiniGameEngine {

    class GLSLProgram {
    public:
        GLSLProgram();
        ~GLSLProgram();

        // Compila los shaders desde archivos de texto
        void compileShaders(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

        // Vincula los atributos (ej: "vertexPosition" -> índice 0)
        void addAttribute(const std::string& attributeName);

        // Termina de configurar el shader
        void linkShaders();

        // Activa el shader para usarlo
        void use();
        
        // Desactiva el shader
        void unuse();

        // Busca la ubicación de una variable uniforme en el shader
        GLint getUniformLocation(const std::string& uniformName);

    private:
        // ID del programa en la GPU
        GLuint _programID;
        GLuint _vertexShaderID;
        GLuint _fragmentShaderID;
        
        // Contador de atributos
        int _numAttributes;

        // Función auxiliar para compilar un shader individual
        void compileShader(const std::string& filePath, GLuint id);
    };
}