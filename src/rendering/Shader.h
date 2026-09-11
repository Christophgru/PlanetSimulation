#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <GL/gl.h>

class Shader {
public:
    GLuint id;
    
    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode;
        std::string fragmentCode;
        
        // Read shaders from files
        std::ifstream vertexStream(vertexPath);
        if (!vertexStream.is_open()) {
            throw std::runtime_error("Failed to open vertex shader: " + std::string(vertexPath));
        }
        std::ifstream fragmentStream(fragmentPath);
        if (!fragmentStream.is_open()) {
            throw std::runtime_error("Failed to open fragment shader: " + std::string(fragmentPath));
        }
        
        // Read shaders into strings
        vertexStream.seekg(0, std::ios::end);
        vertexCode.resize(vertexStream.tellg());
        vertexStream.seekg(0, std::ios::beg);
        vertexStream.read(&vertexCode[0], vertexCode.size());
        
        fragmentStream.seekg(0, std::ios::end);
        fragmentCode.resize(fragmentStream.tellg());
        fragmentStream.seekg(0, std::ios::beg);
        fragmentStream.read(&fragmentCode[0], fragmentCode.size());
        
        // Compile vertex shader
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        const char* vertexSource = vertexCode.c_str();
        glShaderSource(vertex, 1, &vertexSource, nullptr);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        
        // Compile fragment shader
        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fragmentSource = fragmentCode.c_str();
        glShaderSource(fragment, 1, &fragmentSource, nullptr);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        
        // Create shader program
        id = glCreateProgram();
        glAttachShader(id, vertex);
        glAttachShader(id, fragment);
        glLinkProgram(id);
        checkLinkErrors(id);
    }
    
    void use() const {
        glUseProgram(id);
    }
    
    void setFloat(const char* name, float value) const {
        glUniform1f(glGetUniformLocation(id, name), value);
    }
    
    void setFloat2(const char* name, float x, float y) const {
        glUniform2f(glGetUniformLocation(id, name), x, y);
    }
    
    void setFloat3(const char* name, float x, float y, float z) const {
        glUniform3f(glGetUniformLocation(id, name), x, y, z);
    }
    
    void setFloat4(const char* name, float x, float y, float z, float w) const {
        glUniform4f(glGetUniformLocation(id, name), x, y, z, w);
    }
    
    void setMat4(const char* name, const float* value) const {
        glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, value);
    }

private:
    static void checkCompileErrors(GLuint shader, const char* type) {
        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation error (" << type << "):\n" << infoLog << "\n";
            glDeleteShader(shader);
        }
    }
    
    static void checkLinkErrors(GLuint program) {
        GLint success;
        GLchar infoLog[512];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Shader linking error:\n" << infoLog << "\n";
            glDeleteProgram(program);
        }
    }
};
