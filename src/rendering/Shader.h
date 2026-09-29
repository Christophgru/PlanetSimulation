#pragma once

#include <string>
#include <GL/glew.h>

class Shader {
public:
    GLuint id;
    
    Shader(const char* vertexPath, const char* fragmentPath,
           const char* fragmentLibraryPath = nullptr,
           const char* additionalFragmentLibraryPath = nullptr);
    
    void use() const;
    
    void setFloat(const char* name, float value) const;

    void setInt(const char* name, int value) const;
    
    void setFloat2(const char* name, float x, float y) const;
    
    void setFloat3(const char* name, float x, float y, float z) const;
    
    void setFloat4(const char* name, float x, float y, float z, float w) const;
    
    void setMat4(const char* name, const float* value) const;

private:
    static void checkCompileErrors(GLuint shader, const char* type);
    
    static void checkLinkErrors(GLuint program);
};
