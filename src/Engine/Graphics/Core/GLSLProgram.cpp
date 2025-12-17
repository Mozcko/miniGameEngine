#include "GLSLProgram.h"
#include <fstream>
#include <vector>
#include <iostream>

namespace MiniGameEngine {

    GLSLProgram::GLSLProgram() : _programID(0), _vertexShaderID(0), _fragmentShaderID(0), _numAttributes(0) {
    }

    GLSLProgram::~GLSLProgram() {
        // Limpiar memoria de video si es necesario
    }

    void GLSLProgram::compileShaders(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
        // 1. Pedir a OpenGL que cree los IDs
        _programID = glCreateProgram();
        _vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
        if (_vertexShaderID == 0) {
            std::cerr << "Error creando vertex shader!\n";
        }
        _fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
         if (_fragmentShaderID == 0) {
            std::cerr << "Error creando fragment shader!\n";
        }

        // 2. Compilar cada uno
        compileShader(vertexShaderPath, _vertexShaderID);
        compileShader(fragmentShaderPath, _fragmentShaderID);
    }

    void GLSLProgram::linkShaders() {
        // 1. Adjuntar shaders al programa
        glAttachShader(_programID, _vertexShaderID);
        glAttachShader(_programID, _fragmentShaderID);

        // 2. Linkear (Enlazar) el programa
        glLinkProgram(_programID);

        // 3. Verificar errores de enlace
        GLint isLinked = 0;
        glGetProgramiv(_programID, GL_LINK_STATUS, (int*)&isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(_programID, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<char> infoLog(maxLength);
            glGetProgramInfoLog(_programID, maxLength, &maxLength, &infoLog[0]);
            
            // Borramos el programa fallido
            glDeleteProgram(_programID);
            glDeleteShader(_vertexShaderID);
            glDeleteShader(_fragmentShaderID);

            std::cerr << "Error al enlazar shaders: " << &(infoLog[0]) << std::endl;
            return;
        }

        // 4. Limpiar los shaders individuales, ya están en el programa
        glDetachShader(_programID, _vertexShaderID);
        glDetachShader(_programID, _fragmentShaderID);
        glDeleteShader(_vertexShaderID);
        glDeleteShader(_fragmentShaderID);
    }

    void GLSLProgram::addAttribute(const std::string& attributeName) {
        // Asigna un índice (0, 1, 2...) a un nombre de atributo ("vertexPosition")
        glBindAttribLocation(_programID, _numAttributes++, attributeName.c_str());
    }

    void GLSLProgram::use() {
        glUseProgram(_programID);
        // Habilitar los atributos (por ahora solo el 0 -> posición)
        for (int i = 0; i < _numAttributes; i++) {
            glEnableVertexAttribArray(i);
        }
    }

    void GLSLProgram::unuse() {
        glUseProgram(0);
        for (int i = 0; i < _numAttributes; i++) {
            glDisableVertexAttribArray(i);
        }
    }

    void GLSLProgram::compileShader(const std::string& filePath, GLuint id) {
        // A. Leer el archivo
        std::ifstream shaderFile(filePath);
        if (shaderFile.fail()) {
            perror(filePath.c_str());
            std::cerr << "No se pudo abrir el archivo: " << filePath << "\n";
            exit(1);
        }

        std::string fileContents = "";
        std::string line;
        while (std::getline(shaderFile, line)) {
            fileContents += line + "\n";
        }
        shaderFile.close();

        // B. Entregar el código a OpenGL
        const char* contentsPtr = fileContents.c_str();
        glShaderSource(id, 1, &contentsPtr, nullptr);

        // C. Compilar
        glCompileShader(id);

        // D. Verificar errores de compilación
        GLint success = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<char> errorLog(maxLength);
            glGetShaderInfoLog(id, maxLength, &maxLength, &errorLog[0]);
            
            std::cerr << "Fallo al compilar shader " << filePath << ":\n" << &(errorLog[0]) << std::endl;
            glDeleteShader(id);
            return;
        }
    }
    
    GLint GLSLProgram::getUniformLocation(const std::string& uniformName) {
        GLint location = glGetUniformLocation(_programID, uniformName.c_str());
        if (location == GL_INVALID_INDEX) {
            std::cerr << "Uniform " << uniformName << " no encontrado en el shader." << std::endl;
            return -1;
        }
        return location;
    }
}